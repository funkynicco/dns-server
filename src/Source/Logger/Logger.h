#pragma once

#include "StdAfx.h"
#include "Networking\NetworkDefines.h"

typedef struct _LOGGER_CONTEXT
{
    CRITICAL_SECTION cs;
    SOCKET socket;
    SOCKADDR_IN addr;
    LONGLONG id;
    BOOL isFirst;

} LOGGER_CONTEXT, *LPLOGGER_CONTEXT;
extern LOGGER_CONTEXT g_LoggerContext;

void LoggerInitialize();
void LoggerDestroy();

inline void LoggerWriteA(const char* filename, int line, const char* format, ...)
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

    EnterCriticalSection(&g_LoggerContext.cs);

    va_list l;
    va_start(l, format);

    int len = _vscprintf(format, l);

    len += fileAndLineLength;

    if (len < MAX_LOG_MESSAGE)
    {
        LPLOG_MESSAGE lpMessage = (LPLOG_MESSAGE)&buffer[0];

        lpMessage->dwThread = GetCurrentThreadId();
        lpMessage->Flags = LOGFLAG_NONE;

        if (g_LoggerContext.isFirst)
        {
            lpMessage->Flags |= LOGFLAG_SET_LAST_ID;
            g_LoggerContext.isFirst = FALSE;
        }

        lpMessage->Id = InterlockedIncrement64(&g_LoggerContext.id);
        lpMessage->tmTime = time(NULL);

        char* ptr = lpMessage->Message;

        memcpy(ptr, fileAndLineStr, fileAndLineLength);
        ptr += fileAndLineLength;
        ptr += vsprintf(ptr, format, l);
        lpMessage->dwLength = (DWORD)len;

        sendto(g_LoggerContext.socket, buffer, MIN_LOG_MESSAGE_SIZE + lpMessage->dwLength, 0, (CONST LPSOCKADDR)&g_LoggerContext.addr, sizeof(g_LoggerContext.addr));
    }
    else
        ConsoleWrite(LT_WARNING, __FUNCTION__ " - Too big log message to send to log server!!");

    va_end(l);

    LeaveCriticalSection(&g_LoggerContext.cs);
}

#define ErrorLineToStr( x ) #x
#define Error(__msg, ...) { LoggerWriteA(__FILE__, __LINE__, __msg, __VA_ARGS__); ConsoleWrite(LT_NORMAL, __msg, __VA_ARGS__); }

#define LoggerWrite(__format, ...) LoggerWriteA(__FILE__, __LINE__, __format, __VA_ARGS__)