#include "StdAfx.h"
#include "Server.h"

//SOCKET g_socketPool[16];
#define MAX_SOCKETS 16
struct
{
	CRITICAL_SECTION CritSect;
	SOCKET Sockets[MAX_SOCKETS];
	DWORD dwNumberOfSockets;
	DWORD dwAllocatedSockets;

} SocketPool;

void InitializeSocketPool()
{
	ZeroMemory(&SocketPool, sizeof(SocketPool));
	InitializeCriticalSection(&SocketPool.CritSect);

	SocketPoolFill();
}

void DestroySocketPool()
{
	DeleteCriticalSection(&SocketPool.CritSect);

	if (SocketPool.dwAllocatedSockets > 0)
		Error(__FUNCTION__ " - [WARNING] %u sockets are still allocated!", SocketPool.dwAllocatedSockets);

#ifdef __LOG_SOCKET_ALLOCATIONS
	if (SocketPool.dwNumberOfSockets)
		LoggerWrite(__FUNCTION__ " - Closed %u sockets", SocketPool.dwNumberOfSockets);
#endif // __LOG_SOCKET_ALLOCATIONS
	while (SocketPool.dwNumberOfSockets)
		closesocket(SocketPool.Sockets[--SocketPool.dwNumberOfSockets]);
}

SOCKET SocketPoolAllocateSocket()
{
	SOCKET Socket = INVALID_SOCKET;
	EnterCriticalSection(&SocketPool.CritSect);

	if (SocketPool.dwNumberOfSockets > 0)
		Socket = SocketPool.Sockets[--SocketPool.dwNumberOfSockets];

	if (Socket == INVALID_SOCKET)
	{
		Socket = WSASocket(AF_INET, SOCK_DGRAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

		if (Socket == INVALID_SOCKET)
		{
			LeaveCriticalSection(&SocketPool.CritSect);
			Error(__FUNCTION__ " - WSASocket returned INVALID_SOCKET, code: %u - %s", GetLastError(), GetErrorMessage(GetLastError()));
			return INVALID_SOCKET;
		}
	}

	++SocketPool.dwAllocatedSockets;

#ifdef __LOG_SOCKET_ALLOCATIONS
	LoggerWrite(__FUNCTION__ " - Allocated socket %u", Socket);
#endif // __LOG_SOCKET_ALLOCATIONS

	LeaveCriticalSection(&SocketPool.CritSect);

	return Socket;
}

void SocketPoolDestroySocket(SOCKET Socket)
{
	EnterCriticalSection(&SocketPool.CritSect);
	if (SocketPool.dwAllocatedSockets == 0)
		Error(__FUNCTION__ " - dwAllocatedSockets is 0 in attempt to destroy socket: %u", Socket)
	else
		--SocketPool.dwAllocatedSockets;
	LeaveCriticalSection(&SocketPool.CritSect);

	// there doesn't seem to be any way to reuse a socket except in for example AcceptEx
	closesocket(Socket);

#ifdef __LOG_SOCKET_ALLOCATIONS
	LoggerWrite(__FUNCTION__ " - Destroyed socket %u", Socket);
#endif // __LOG_SOCKET_ALLOCATIONS
}

void SocketPoolFill()
{
#ifdef __LOG_SOCKET_ALLOCATIONS
	char str[256];
	char* p = str;

	LARGE_INTEGER liFrequency, liStart, liEnd;
	QueryPerformanceFrequency(&liFrequency);
#endif // __LOG_SOCKET_ALLOCATIONS

	EnterCriticalSection(&SocketPool.CritSect);
#ifdef __LOG_SOCKET_ALLOCATIONS
	QueryPerformanceCounter(&liStart);
#endif // __LOG_SOCKET_ALLOCATIONS
	for (int i = SocketPool.dwNumberOfSockets; i < MAX_SOCKETS; ++i)
	{
		SOCKET Socket = WSASocket(AF_INET, SOCK_DGRAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (Socket == INVALID_SOCKET)
		{
			Error(__FUNCTION__ " - WSASocket failed with code: %u - %s", WSAGetLastError(), GetErrorMessage(WSAGetLastError()));
			break;
		}

		SocketPool.Sockets[SocketPool.dwNumberOfSockets++] = Socket;
		
#ifdef __LOG_SOCKET_ALLOCATIONS
		if (p > str)
			*p++ = ' ';
		p += sprintf(p, "%u", Socket);
#endif // __LOG_SOCKET_ALLOCATIONS
	}

#ifdef __LOG_SOCKET_ALLOCATIONS
	QueryPerformanceCounter(&liEnd);
#endif // __LOG_SOCKET_ALLOCATIONS

	LeaveCriticalSection(&SocketPool.CritSect);

#ifdef __LOG_SOCKET_ALLOCATIONS
	if (p > str)
	{
		*p = 0;

		char strtime[128];
		GetFrequencyCounterResult(strtime, liFrequency, liStart, liEnd);

		LoggerWrite(__FUNCTION__ " - Created socket(s) [%s]: %s", strtime, str);
	}
#endif // __LOG_SOCKET_ALLOCATIONS
}