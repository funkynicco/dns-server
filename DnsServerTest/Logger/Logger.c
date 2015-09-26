#include "StdAfx.h"
#include "Logger.h"

static TCHAR g_szFilename[MAX_PATH];
static TCHAR g_szFilenameHtml[MAX_PATH];
static CRITICAL_SECTION g_cs;

void LoggerInitialize()
{
	SYSTEMTIME st;
	GetLocalTime(&st);

	CreateDirectory(L"Logs", NULL);

	TCHAR szCurrentDirectory[MAX_PATH];
	TCHAR szName[64];

	GetCurrentDirectory(MAX_PATH, szCurrentDirectory);
	wsprintf(
		szName,
		L"Logs\\%04d.%02d.%02d_%02d.%02d.%02d.txt",
		st.wYear,
		st.wMonth,
		st.wDay,
		st.wHour,
		st.wMinute,
		st.wSecond);

	PathCombine(g_szFilename, szCurrentDirectory, szName);
	PathCombine(g_szFilenameHtml, szCurrentDirectory, L"Logs\\log.html");

	InitializeCriticalSection(&g_cs);
}

void LoggerDestroy()
{
	DeleteCriticalSection(&g_cs);
}

static void ReadAndWrite(HANDLE hSourceFile, HANDLE hDestinationFile)
{
	char buffer[4096];
	DWORD dwFileSize = GetFileSize(hSourceFile, NULL);
	DWORD dwPos = 0;
	DWORD dw;

	while (dwPos < dwFileSize)
	{
		if (!ReadFile(hSourceFile, buffer, min(sizeof(buffer), dwFileSize - dwPos), &dw, NULL))
			break;

		dwPos += dw;

		if (!WriteFile(hDestinationFile, buffer, dw, &dw, NULL))
			break;
	}
}

static void LoadAndWrite(HANDLE hDestinationFile, LPCWSTR filename)
{
	HANDLE hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return;

	ReadAndWrite(hFile, hDestinationFile);
	CloseHandle(hFile);
}

static void GenerateHtmlFile(HANDLE hLogFile)
{
	HANDLE hFile = CreateFile(g_szFilenameHtml, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		SetFilePointer(hLogFile, 0, 0, FILE_BEGIN);
		DWORD dwFileSize = GetFileSize(hLogFile, NULL);

		LoadAndWrite(hFile, L"Logs\\template\\header.html");
		ReadAndWrite(hLogFile, hFile);
		LoadAndWrite(hFile, L"Logs\\template\\footer.html");

		CloseHandle(hFile);
	}
}

void LoggerWriteA(const char* filename, int line, const char* format, ...)
{
	const char RemoveFilepathPart[] = "D:\\Coding\\CPP\\VS2015\\Test\\DnsServerTest\\DnsServerTest\\";;
	const size_t RemoveFilepathPartSize = sizeof(RemoveFilepathPart) - 1;

	char newFilename[MAX_PATH];
	char* newFilenamePtr = newFilename;

	size_t filenameLength = strlen(filename);
	if (filenameLength >= RemoveFilepathPartSize &&
		_memicmp(filename, RemoveFilepathPart, RemoveFilepathPartSize) == 0)
	{
		filename += RemoveFilepathPartSize;
		filenameLength -= RemoveFilepathPartSize;
		*newFilenamePtr++ = '/';
	}

	newFilenamePtr += sprintf(newFilenamePtr, "%s:%d", filename, line);
	for (char* p = newFilename; *p; ++p)
	{
		if (*p == '\\')
			*p = '/';
	}

	size_t newfilenameLength = newFilenamePtr - newFilename;

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	char buffer[8192];

	va_list l;
	va_start(l, format);

	int len = _vscprintf(format, l);

	len += 256 + newfilenameLength;

	char* buf = buffer;
	if (len + 1 > sizeof(buffer))
		buf = (char*)malloc(len + 1);

	SYSTEMTIME st;
	GetLocalTime(&st);
	int offset = sprintf(
		buf,
		"<tr><td>%04d/%02d/%02d %02d:%02d:%02d</td><td>%u</td><td>%s</td><td>",
		st.wYear,
		st.wMonth,
		st.wDay,
		st.wHour,
		st.wMinute,
		st.wSecond,
		GetCurrentThreadId(),
		newFilename);

	offset += vsprintf(buf + offset, format, l);
	//memcpy(buf + offset, "\r\n", 2); offset += 2;
	offset += sprintf(buf + offset, "</td></tr>");

	va_end(l);

	struct potato
	{
		int a;
		int b;
	};

	struct potato banana;
	banana.a = 1;

	EnterCriticalSection(&g_cs);
	HANDLE hFile = CreateFile(g_szFilename, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		SetFilePointer(hFile, 0, 0, FILE_END);

		DWORD dwPos = 0;
		DWORD dwLength = offset;
		DWORD dwWritten;

		while (dwPos < dwLength)
		{
			if (!WriteFile(hFile, buf + dwPos, dwLength - dwPos, &dwWritten, NULL))
			{
				printf(__FUNCTION__ " - Failed to write log file, code: %u - %s\n", GetLastError(), GetErrorMessage(GetLastError()));
				break;
			}

			dwPos += dwWritten;
		}

		GenerateHtmlFile(hFile);

		CloseHandle(hFile);
	}
	else
		printf(__FUNCTION__ " - Failed to open log file, code: %u - %s\n", GetLastError(), GetErrorMessage(GetLastError()));
	LeaveCriticalSection(&g_cs);

	if (buf != buffer)
		free(buf);
}