#include "StdAfx.h"
#include "Logger.h"

Logger::Logger()
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

	PathCombine(m_szFilename, szCurrentDirectory, szName);
	PathCombine(m_szFilenameHtml, szCurrentDirectory, L"Logs\\log.html");

	InitializeCriticalSectionAndSpinCount(&m_cs, 2000);
}

Logger::~Logger()
{
	DeleteCriticalSection(&m_cs);
}

void Logger::Write(time_t tmTime, const char* filename, int line, LPCSTR szText)
{
	Write(tmTime, filename, line, szText, strlen(szText));
}

void Logger::Write(time_t tmTime, const char* filename, int line, LPCSTR szText, size_t nTextLength)
{
	char newFilename[MAX_PATH];
	char* newFilenamePtr = newFilename;

	newFilenamePtr += sprintf(newFilenamePtr, "/%s:%d", filename, line);
	for (char* p = newFilename; *p; ++p)
	{
		if (*p == '\\')
			*p = '/';
	}

	size_t newfilenameLength = newFilenamePtr - newFilename;

	//////////////////////////////////////////////////////////////////////////

	char buffer[8192];

	size_t len = nTextLength;

	len += 256 + newfilenameLength;

	char* buf = buffer;
	if (len + 1 > sizeof(buffer))
		buf = (char*)malloc(len + 1);

	char timeBuf[64];
	strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", localtime(&tmTime));

	int offset = sprintf(
		buf,
		"<tr><td>%s</td><td>%u</td><td>%s</td><td>",
		timeBuf,
		GetCurrentThreadId(),
		newFilename);

	memcpy(buf + offset, szText, nTextLength);
	offset += (int)nTextLength;
	offset += sprintf(buf + offset, "</td></tr>");

	EnterCriticalSection(&m_cs);

	HANDLE hFile = CreateFile(m_szFilename, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
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

	LeaveCriticalSection(&m_cs);

	if (buf != buffer)
		free(buf);
}

void Logger::WriteFormat(time_t tmTime, const char* filename, int line, LPCSTR szFormat, ...)
{
	char buffer[8192];

	va_list l;
	va_start(l, szFormat);

	int len = _vscprintf(szFormat, l);

	char* buf = buffer;
	if (len + 1 > sizeof(buffer))
		buf = (char*)malloc(len + 1);

	vsprintf(buf, szFormat, l);
	
	va_end(l);

	Write(tmTime, filename, line, buf, len);

	if (buf != buffer)
		free(buf);
}

void Logger::ReadAndWrite(HANDLE hSourceFile, HANDLE hDestinationFile)
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

void Logger::LoadAndWrite(HANDLE hDestinationFile, LPCWSTR filename)
{
	HANDLE hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return;

	ReadAndWrite(hFile, hDestinationFile);
	CloseHandle(hFile);
}

void Logger::GenerateHtmlFile(HANDLE hLogFile)
{
	HANDLE hFile = CreateFile(m_szFilenameHtml, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
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