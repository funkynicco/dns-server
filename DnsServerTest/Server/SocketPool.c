#include "StdAfx.h"
#include "Server.h"

//SOCKET g_socketPool[16];
#define MAX_SOCKETS 256
struct
{
	CRITICAL_SECTION CritSect;
	//SOCKET Sockets[MAX_SOCKETS];
	//DWORD dwNumberOfSockets;
	DWORD dwAllocatedSockets;

} SocketPool;

void InitializeSocketPool()
{
	ZeroMemory(&SocketPool, sizeof(SocketPool));
	InitializeCriticalSection(&SocketPool.CritSect);
}

void DestroySocketPool()
{
	DeleteCriticalSection(&SocketPool.CritSect);

	if (SocketPool.dwAllocatedSockets > 0)
		Error(__FUNCTION__ " - %u sockets are still allocated!", SocketPool.dwAllocatedSockets);
}

SOCKET AllocateSocket()
{
	EnterCriticalSection(&SocketPool.CritSect);
	++SocketPool.dwAllocatedSockets;
	LeaveCriticalSection(&SocketPool.CritSect);

	SOCKET Socket = WSASocket(AF_INET, SOCK_DGRAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	if (Socket == INVALID_SOCKET)
	{
		Error(__FUNCTION__ " - WSASocket returned INVALID_SOCKET, code: %u - %s", GetLastError(), GetErrorMessage(GetLastError()));
		return INVALID_SOCKET;
	}

#ifdef __LOG_SOCKET_ALLOCATIONS
	LoggerWrite(__FUNCTION__ " - Allocated socket %u", Socket);
#endif // __LOG_SOCKET_ALLOCATIONS

	return Socket;
}

void DestroySocket(SOCKET Socket)
{
	EnterCriticalSection(&SocketPool.CritSect);
	if (SocketPool.dwAllocatedSockets == 0)
		Error(__FUNCTION__ " - dwAllocatedSockets is 0 in attempt to destroy socket: %u", Socket)
	else
		--SocketPool.dwAllocatedSockets;
	LeaveCriticalSection(&SocketPool.CritSect);

	closesocket(Socket);

#ifdef __LOG_SOCKET_ALLOCATIONS
	LoggerWrite(__FUNCTION__ " - Destroyed socket %u", Socket);
#endif // __LOG_SOCKET_ALLOCATIONS
}