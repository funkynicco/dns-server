#pragma once

typedef struct _tagWebClientInfo
{
	WSAOVERLAPPED Overlapped;

	struct _tagWebServerInfo* lpServerInfo;
	SOCKADDR_IN SocketAddress;
	int SockAddrLen;

	char Buffer[1024];
	DWORD dwLength;
	DWORD dwFlags;

	int IOMode;
	SOCKET Socket;

	struct _tagWebClientInfo* next;

} WEB_CLIENT_INFO, *LPWEB_CLIENT_INFO;

typedef struct _tagWebServerInfo
{
	SOCKET ListenSocket;
	IO_THREADS_INFO IOThreads;

	LPWEB_CLIENT_INFO lpFreeClients;
	DWORD dwAllocatedClients;
	CRITICAL_SECTION csAllocClient;

	LPARRAY_CONTAINER lpPendingWSARecv;
	LPARRAY_CONTAINER lpPendingWSASend;
	LPARRAY_CONTAINER lpAllocatedClients;
	CRITICAL_SECTION csStats;

} WEB_SERVER_INFO, *LPWEB_SERVER_INFO;

/***************************************************************************************
 * WebAllocation.c
 ***************************************************************************************/

LPWEB_SERVER_INFO	AllocateWebServerInfo(DWORD dwNumberOfThreads);
void				DestroyWebServerInfo(LPWEB_SERVER_INFO lpServerInfo);
LPWEB_CLIENT_INFO	AllocateWebClientInfo(LPWEB_SERVER_INFO lpServerInfo, SOCKET Socket);
void				DestroyWebClientInfo(LPWEB_CLIENT_INFO lpClientInfo);

/***************************************************************************************
 * WebServer.c
 ***************************************************************************************/

BOOL WebServerStart(LPWEB_SERVER_INFO lpServerInfo);
void WebServerStop(LPWEB_SERVER_INFO lpServerInfo);

/***************************************************************************************
 * WebServerIO.c
 ***************************************************************************************/

