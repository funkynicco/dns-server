#include "StdAfx.h"
#include "Logger.h"

static CRITICAL_SECTION g_cs;
static SOCKET g_socket;
static SOCKADDR_IN g_addr;
static LONGLONG g_id;
static BOOL g_isFirst;

void LoggerInitialize()
{
	InitializeCriticalSection(&g_cs);

	g_socket = WSASocket(AF_INET, SOCK_DGRAM, 0, NULL, 0, 0);
	if (g_socket != INVALID_SOCKET)
	{
		MakeSocketAddress(&g_addr, "127.0.0.1", 16650);
	}

	g_id = 0;
	g_isFirst = TRUE;
}

void LoggerDestroy()
{
	DeleteCriticalSection(&g_cs);
	if (g_socket != INVALID_SOCKET)
		closesocket(g_socket);
}

void LoggerWriteA(const char* filename, int line, const char* format, ...)
{
	const char RemoveFilepathPart[] = "D:\\Coding\\CPP\\VS2015\\Test\\DnsServerTest\\DnsServerTest\\";;
	const size_t RemoveFilepathPartSize = sizeof(RemoveFilepathPart) - 1;

	size_t filenameLength = strlen(filename);
	if (filenameLength >= RemoveFilepathPartSize &&
		_memicmp(filename, RemoveFilepathPart, RemoveFilepathPartSize) == 0)
	{
		filename += RemoveFilepathPartSize;
		filenameLength -= RemoveFilepathPartSize;
	}

	char fileAndLineStr[MAX_PATH + 128];
	int fileAndLineLength = sprintf(fileAndLineStr, "%s;%d;", filename, line);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	char buffer[8192];

	EnterCriticalSection(&g_cs);

	va_list l;
	va_start(l, format);

	int len = _vscprintf(format, l);

	len += fileAndLineLength;

	if (len < MAX_LOG_MESSAGE)
	{
		LPLOG_MESSAGE lpMessage = (LPLOG_MESSAGE)&buffer[0];

		lpMessage->dwThread = GetCurrentThreadId();
		lpMessage->Flags = LOGFLAG_NONE;

		if (g_isFirst)
		{
			lpMessage->Flags |= LOGFLAG_SET_LAST_ID;
			g_isFirst = FALSE;
		}

		lpMessage->Id = InterlockedIncrement64(&g_id);
		lpMessage->tmTime = time(NULL);

		char* ptr = lpMessage->Message;

		memcpy(ptr, fileAndLineStr, fileAndLineLength);
		ptr += fileAndLineLength;
		ptr += vsprintf(ptr, format, l);
		lpMessage->dwLength = (DWORD)len;

		sendto(g_socket, buffer, MIN_LOG_MESSAGE_SIZE + lpMessage->dwLength, 0, (CONST LPSOCKADDR)&g_addr, sizeof(g_addr));
	}
	else
		printf(__FUNCTION__ " - Too big log message to send to log server!!\n");

	va_end(l);
}