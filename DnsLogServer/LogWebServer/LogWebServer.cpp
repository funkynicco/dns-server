#include "StdAfx.h"
#include "LogWebServer.h"

LogWebServer::LogWebServer() :
	m_pFreeBuffers(NULL)
{

}

LogWebServer::~LogWebServer()
{
	while (m_pFreeBuffers)
	{
		LPLARGE_BUFFER lpBuffer = m_pFreeBuffers;
		m_pFreeBuffers = m_pFreeBuffers->next;
		free(lpBuffer);
	}
}

void LogWebServer::OnClientConnected(LPCLIENT lpClient)
{
	LPWEB_CLIENT_INFO lpClientInfo = static_cast<LPWEB_CLIENT_INFO>(lpClient->Tag = new WEB_CLIENT_INFO);

	lpClientInfo->lpClient = lpClient;
	lpClientInfo->bIsHandshake = TRUE;
	lpClientInfo->tmConnect = time(NULL);
	if (!CircularBufferCreate(&lpClientInfo->Buffer, 8192))
		exit(1);

	//printf(__FUNCTION__ " - Connection from %s (socket: %Id)\n", lpClient->szIP, lpClient->Socket);
}

void LogWebServer::OnClientDisconnected(LPCLIENT lpClient)
{
	LPWEB_CLIENT_INFO lpClientInfo = static_cast<LPWEB_CLIENT_INFO>(lpClient->Tag);

	if (lpClientInfo)
	{
		//printf(__FUNCTION__ " - %s (socket: %Id) has disconnected\n", lpClient->szIP, lpClient->Socket);

		CircularBufferDestroy(&lpClientInfo->Buffer);
		delete lpClientInfo;
	}
}

void LogWebServer::OnClientData(LPCLIENT lpClient, const char* buffer, int size)
{
	LPWEB_CLIENT_INFO lpClientInfo = static_cast<LPWEB_CLIENT_INFO>(lpClient->Tag);

	//printf(__FUNCTION__ " - Received %u bytes from %s (socket: %Id)\n", size, lpClient->szIP, lpClient->Socket);

	CircularBufferAppend(&lpClientInfo->Buffer, buffer, size);

	DWORD dwDataOffset; // also the size of the header (including ending \r\n\r\n)
	auto result = ParseHttpHeader(&lpClientInfo->Http, &lpClientInfo->Buffer, &dwDataOffset);
	if (result != PHR_SUCCEEDED)
	{
		printf(__FUNCTION__ " - (%s - socket: %Id) ParseHttpHeader returned %s\n", lpClient->szIP, lpClient->Socket, GetParseHeaderResultText(result));
		if (result == PHR_INVALID_HEADER)
			lpClient->Disconnect();

		if (result == PHR_NEED_MORE_DATA)
		{
			// write data received
			char data[65536];
			memcpy(data, buffer, size);
			data[size + 0] = '\n';
			data[size + 1] = 0;
			OutputDebugStringA(data);
		}

		return;
	}

	CircularBufferRemove(&lpClientInfo->Buffer, dwDataOffset); // if any data, then we need to remove it too ...

	//unordered_map<string, string>::iterator it = lpClientInfo->Http.parameters.find("content-length");

	HandleRequest(lpClientInfo);
}

void LogWebServer::HandleRequest(LPWEB_CLIENT_INFO lpWebClientInfo)
{
	if (_strcmpi(lpWebClientInfo->Http.szMethod, "get") != 0)
	{
		unordered_map<string, string> extra_parameters;
		extra_parameters.insert(pair<string, string>("Allow-Header", "GET"));
		lpWebClientInfo->SendCodeResult(405, "Method Not Allowed", extra_parameters, "Unsupported request method.");
		lpWebClientInfo->lpClient->Disconnect();
		return;
	}

	// dynamic interception here ...
	if (HandleDynamicRequest(lpWebClientInfo))
		return;

	// static files
	const char* RootDirectory = "d:\\coding\\cpp\\vs2015\\test\\dnsservertest\\dnslogserver\\www"; // must be lowercase
	size_t RootDirectoryLength = strlen(RootDirectory);

	// normalize the path (move this to a utilitiy function...)
	char szPath[256];
	char* p1 = szPath;
	const char* p2 = lpWebClientInfo->Http.szPath;
	BOOL bIsFirstCharInUrl = TRUE;
	while (*p2)
	{
		char c = *p2;

		switch (c)
		{
		case '/':
			if (bIsFirstCharInUrl)
				c = 0;
			else
				c = '\\';
			break;
		default:
			bIsFirstCharInUrl = FALSE;
			break;
		}

		if (c)
			*p1++ = c;

		++p2;
	}

	*p1 = 0;

	char filepath[MAX_PATH];
	PathCombineA(filepath, RootDirectory, szPath);
	printf(__FUNCTION__ " - Request physical file: '%s'\n", filepath);

	// check if it's valid
	size_t filepath_len = strlen(filepath);
	if (filepath_len < RootDirectoryLength ||
		memcmp(filepath, RootDirectory, RootDirectoryLength) != 0)
	{
		printf(__FUNCTION__ " - Invalid request!\n");
		lpWebClientInfo->SendNotFoundPage();
		lpWebClientInfo->lpClient->Disconnect();
		return;
	}

	const char* pszExtension = PathFindExtensionA(filepath);
	if (*pszExtension == 0)
	{
		char tempPath[MAX_PATH];
		PathCombineA(tempPath, filepath, "index.html");
		if (PathFileExistsA(tempPath))
		{
			HandleStaticRequest(lpWebClientInfo, tempPath);
			return;
		}

		// directory listing

		lpWebClientInfo->SendNotFoundPage();
		return;
	}

	/*char html[2048];
	sprintf(html, "Requested physical file: '%s'", filepath);

	unordered_map<string, string> extra_parameters;
	lpWebClientInfo->SendCodeResult(200, "OK", extra_parameters, html);*/

	HandleStaticRequest(lpWebClientInfo, filepath);
}

void LogWebServer::HandleStaticRequest(LPWEB_CLIENT_INFO lpWebClientInfo, const char* filepath)
{
	HANDLE hFile = CreateFileA(
		filepath,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (hFile == INVALID_HANDLE_VALUE)
	{
		lpWebClientInfo->SendNotFoundPage();
		return;
	}

	char filename[256];
	strcpy(filename, filepath);
	PathStripPathA(filename);

	LARGE_INTEGER FileSize;
	GetFileSizeEx(hFile, &FileSize);

	const char* pszExtension = PathFindExtensionA(filename);
	//if (!pszExtension)
	//{
	//	// DIRECTORY REQUEST!!
	//	printf(__FUNCTION__ " - [Error] Directory request '%s'\n", filename);
	//	lpWebClientInfo->SendNotFoundPage();
	//	return;
	//}

	if (*pszExtension)
		++pszExtension; // skip the prefixed extension dot

	LPMIME_TYPE lpMimeType = g_cfg.GetMimeType(pszExtension);

	if (!lpMimeType)
	{
		printf(__FUNCTION__ " - Mime type not found for extension: '%s'\n", pszExtension);
		lpWebClientInfo->SendNotFoundPage();
		return;
	}

	// send the header info
	char header[16384];
	char* pheader = header;
	pheader += sprintf(
		pheader,
		"HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %I64d\r\n",
		lpMimeType->szMimeType,
		FileSize.QuadPart);

	if (lpMimeType->bIsContentDisposition)
	{
		pheader += sprintf(
			pheader,
			"Content-Disposition: attachment; filename=\"%s\"\r\n",
			filename);
	}

	*pheader++ = '\r';
	*pheader++ = '\n';
	*pheader = 0;

	lpWebClientInfo->lpClient->Send(header, int(pheader - header));

	LARGE_INTEGER Position = {};
	DWORD dwRead;

	char buffer[65536];

	while (Position.QuadPart < FileSize.QuadPart)
	{
		if (!ReadFile(hFile, buffer, (DWORD)min(sizeof(buffer), FileSize.QuadPart - Position.QuadPart), &dwRead, NULL))
			break;

		lpWebClientInfo->lpClient->Send(buffer, (int)dwRead); // TODO: Send should return a result value to determine if client stopped download ...

		Position.QuadPart += dwRead;
	}

	CloseHandle(hFile);

	if (Position.QuadPart != FileSize.QuadPart)
	{
		lpWebClientInfo->lpClient->Disconnect();
		printf(__FUNCTION__ " - Failed to send full file '%s'\n", filepath);
	}
}

BOOL LogWebServer::HandleDynamicRequest(LPWEB_CLIENT_INFO lpWebClientInfo)
{
	if (_strcmpi(lpWebClientInfo->Http.szPath, "/api/log") == 0)
	{
		printf(__FUNCTION__ " - /api/log\n");

		JSON_DATA json;
		JsonCreateInstance(&json);
		// json array order: id,thread,time,filename,line,msg

		JsonWriteString(&json, "{\"logs\":[", FALSE);

		LogCache::GetInstance()->Lock();
		map<LONGLONG, LPLOG_ITEM>& logs = LogCache::GetInstance()->GetLogs();
		int n = 0;
		for (map<LONGLONG, LPLOG_ITEM>::const_iterator it = logs.cbegin(); it != logs.cend(); ++it)
		{
			if (n++)
				JsonWriteChar(&json, ',');
			/*pBuf += sprintf(
				pBuf,
				"[%I64d,%u,%I64d,\"%s\",%d,\"%s\"]",
				it->second->LogMessage.Id,
				it->second->LogMessage.dwThread,
				it->second->LogMessage.tmTime,
				it->second->Filename,
				it->second->Line,
				it->second->LogMessage.Message);*/

			JsonWriteChar(&json, '[');
			JsonWriteNumber(&json, it->second->LogMessage.Id);
			JsonWriteChar(&json, ',');
			JsonWriteNumber(&json, it->second->LogMessage.dwThread);
			JsonWriteChar(&json, ',');
			JsonWriteNumber(&json, it->second->LogMessage.tmTime);
			JsonWriteString(&json, ",\"", FALSE);
			JsonWriteString(&json, it->second->Filename, TRUE);
			JsonWriteString(&json, "\",", FALSE);
			JsonWriteNumber(&json, it->second->Line);
			JsonWriteString(&json, ",\"", FALSE);
			JsonWriteString(&json, it->second->LogMessage.Message, TRUE);
			JsonWriteString(&json, "\"]", FALSE);
		}
		LogCache::GetInstance()->Unlock();

		JsonWriteString(&json, "]}", FALSE);

		DWORD dwJsonLength = DWORD(json.lpPtr - json.lpMemBegin);

		char slen[32];
		sprintf(slen, "%u", dwJsonLength);

		unordered_map<string, string> extra_parameters;
		extra_parameters.insert(pair<string, string>("Content-Type", "application/json"));
		extra_parameters.insert(pair<string, string>("Content-Length", slen));

		lpWebClientInfo->SendHeader(200, "OK", extra_parameters);
		lpWebClientInfo->lpClient->Send(json.lpMemBegin, dwJsonLength);

		//DestroyLargeBuffer(lpBuffer);
		JsonDestroyInstance(&json);

		return TRUE;
	}

	return FALSE;
}