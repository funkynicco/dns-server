#include "StdAfx.h"
#include "SocketPool.h"

#define MAX_SOCKETS 16

struct _tagSocketPool
{
	CRITICAL_SECTION CritSect;
	SOCKET Sockets[MAX_SOCKETS];
	DWORD dwNumberOfSockets;
	DWORD dwAllocatedSockets;

	// WSASocket parameters
	struct
	{
		int af;
		int type;
		int protocol;
		DWORD dwFlags;

	} SocketParameters;
};

struct _tagSocketPool SocketPool[SOCKETTYPE_MAX];

#define SetSocketParameters(_Index, _AF, _Type, _Protocol, _Flags) \
	SocketPool[_Index].SocketParameters.af = _AF; \
	SocketPool[_Index].SocketParameters.type = _Type; \
	SocketPool[_Index].SocketParameters.protocol = _Protocol; \
	SocketPool[_Index].SocketParameters.dwFlags = _Flags;

void InitializeSocketPool()
{
	for (int i = 0; i < SOCKETTYPE_MAX; ++i)
	{
		ZeroMemory(&SocketPool[i], sizeof(struct _tagSocketPool));
		InitializeCriticalSection(&SocketPool[i].CritSect);
	}

	SetSocketParameters(SOCKETTYPE_UDP, AF_INET, SOCK_DGRAM, 0, WSA_FLAG_OVERLAPPED);
	SetSocketParameters(SOCKETTYPE_TCP, AF_INET, SOCK_STREAM, IPPROTO_TCP, WSA_FLAG_OVERLAPPED);

	SocketPoolFill();
}

void DestroySocketPool()
{
	for (int i = 0; i < SOCKETTYPE_MAX; ++i)
	{
		DeleteCriticalSection(&SocketPool[i].CritSect);

		if (SocketPool[i].dwAllocatedSockets > 0)
			Error(__FUNCTION__ " - [WARNING] %u %s sockets are still allocated!", SocketPool[i].dwAllocatedSockets, i == SOCKETTYPE_UDP ? "UDP" : "TCP");

#ifdef __LOG_SOCKET_ALLOCATIONS
		if (SocketPool[i].dwNumberOfSockets)
			LoggerWrite(__FUNCTION__ " - Closed %u %s sockets", SocketPool[i].dwNumberOfSockets, i == SOCKETTYPE_UDP ? "UDP" : "TCP");
#endif // __LOG_SOCKET_ALLOCATIONS
		while (SocketPool[i].dwNumberOfSockets)
			closesocket(SocketPool[i].Sockets[--SocketPool[i].dwNumberOfSockets]);
	}
}

SOCKET SocketPoolAllocateSocket(int socketType)
{
	struct _tagSocketPool* pPool = &SocketPool[socketType];

	SOCKET Socket = INVALID_SOCKET;
	EnterCriticalSection(&pPool->CritSect);

	if (pPool->dwNumberOfSockets > 0)
		Socket = pPool->Sockets[--pPool->dwNumberOfSockets];

	if (Socket == INVALID_SOCKET)
	{
		Socket = WSASocket(
			pPool->SocketParameters.af,
			pPool->SocketParameters.type,
			pPool->SocketParameters.protocol,
			NULL,
			0,
			pPool->SocketParameters.dwFlags);

		if (Socket == INVALID_SOCKET)
		{
			LeaveCriticalSection(&pPool->CritSect);
			Error(__FUNCTION__ " - WSASocket returned INVALID_SOCKET, code: %u - %s", GetLastError(), GetErrorMessage(GetLastError()));
			return INVALID_SOCKET;
		}
	}

	++pPool->dwAllocatedSockets;

#ifdef __LOG_SOCKET_ALLOCATIONS
	LoggerWrite(__FUNCTION__ " - Allocated %s socket %u", socketType == SOCKETTYPE_UDP ? "UDP" : "TCP", Socket);
#endif // __LOG_SOCKET_ALLOCATIONS

	LeaveCriticalSection(&pPool->CritSect);

	return Socket;
}

void SocketPoolDestroySocket(int socketType, SOCKET Socket)
{
	struct _tagSocketPool* pPool = &SocketPool[socketType];

	EnterCriticalSection(&pPool->CritSect);
	if (pPool->dwAllocatedSockets == 0)
		Error(__FUNCTION__ " - dwAllocatedSockets is 0 in attempt to destroy %s socket: %Id", socketType == SOCKETTYPE_UDP ? "UDP" : "TCP", Socket)
	else
		--pPool->dwAllocatedSockets;
	LeaveCriticalSection(&pPool->CritSect);

	// there doesn't seem to be any way to reuse a socket except in for example AcceptEx
	closesocket(Socket);

#ifdef __LOG_SOCKET_ALLOCATIONS
	LoggerWrite(__FUNCTION__ " - Destroyed %s socket %Id", socketType == SOCKETTYPE_UDP ? "UDP" : "TCP", Socket);
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

	for (int i = 0; i < SOCKETTYPE_MAX; ++i)
	{
		if (i == SOCKETTYPE_UDP &&
			!g_Configuration.DnsServer.Enabled)
			continue;

		if (i == SOCKETTYPE_TCP &&
			(!g_Configuration.Proxy.Enabled ||
				!g_Configuration.Web.Enabled))
			continue;

		struct _tagSocketPool* pPool = &SocketPool[i];

		EnterCriticalSection(&pPool->CritSect);

#ifdef __LOG_SOCKET_ALLOCATIONS
		QueryPerformanceCounter(&liStart);
#endif // __LOG_SOCKET_ALLOCATIONS

		for (int i = pPool->dwNumberOfSockets; i < MAX_SOCKETS; ++i)
		{
			SOCKET Socket = WSASocket(
				pPool->SocketParameters.af,
				pPool->SocketParameters.type,
				pPool->SocketParameters.protocol,
				NULL,
				0,
				pPool->SocketParameters.dwFlags);
			if (Socket == INVALID_SOCKET)
			{
				Error(__FUNCTION__ " - WSASocket failed with code: %u - %s", WSAGetLastError(), GetErrorMessage(WSAGetLastError()));
				break;
			}

			pPool->Sockets[pPool->dwNumberOfSockets++] = Socket;

#ifdef __LOG_SOCKET_ALLOCATIONS
			if (p > str)
				*p++ = ' ';
			p += sprintf(p, "%Id", Socket);
#endif // __LOG_SOCKET_ALLOCATIONS
		}

#ifdef __LOG_SOCKET_ALLOCATIONS
		QueryPerformanceCounter(&liEnd);
#endif // __LOG_SOCKET_ALLOCATIONS

		LeaveCriticalSection(&pPool->CritSect);
	}

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