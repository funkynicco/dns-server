#include "StdAfx.h"
#include "Logger.h"

LOGGER_CONTEXT g_LoggerContext;

void LoggerInitialize()
{
	InitializeCriticalSection(&g_LoggerContext.cs);

    g_LoggerContext.socket = WSASocket(AF_INET, SOCK_DGRAM, 0, NULL, 0, 0);
	if (g_LoggerContext.socket != INVALID_SOCKET)
	{
		MakeSocketAddress(&g_LoggerContext.addr, "127.0.0.1", 16650);
	}

    g_LoggerContext.id = 0;
    g_LoggerContext.isFirst = TRUE;
}

void LoggerDestroy()
{
	DeleteCriticalSection(&g_LoggerContext.cs);
	if (g_LoggerContext.socket != INVALID_SOCKET)
		closesocket(g_LoggerContext.socket);
}
