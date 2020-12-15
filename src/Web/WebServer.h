#pragma once

#define MAX_WEB_CLIENT_BUFFER 1024

typedef struct _tagWebClientBuffer
{
	WSAOVERLAPPED Overlapped;

	struct _tagWebClientInfo* lpClientInfo;

	char Buffer[MAX_WEB_CLIENT_BUFFER];
	DWORD dwLength;
	DWORD dwFlags;

	int IOMode;

	struct _tagWebClientBuffer* next;

} WEB_CLIENT_BUFFER, *LPWEB_CLIENT_BUFFER;

typedef struct _tagWebClientInfo
{
	struct _tagWebServerInfo* lpServerInfo;
	SOCKADDR_IN SocketAddress;
	int SockAddrLen;

	SOCKET Socket;
	BOOL bFreeze;

	LONG NumberOfBuffers;

	struct _tagWebClientInfo* next;

} WEB_CLIENT_INFO, *LPWEB_CLIENT_INFO;

typedef struct _tagWebServerInfo
{
	SOCKET ListenSocket;
	IO_THREADS_INFO IOThreads;

	LPWEB_CLIENT_INFO lpFreeClients;
	DWORD dwAllocatedClients;
	CRITICAL_SECTION csAllocClient;

	LPWEB_CLIENT_BUFFER lpFreeBuffers;
	DWORD dwAllocatedBuffers;
	CRITICAL_SECTION csAllocBuffer;

	ARRAY_CONTAINER PendingWSARecv;
	ARRAY_CONTAINER PendingWSASend;
	ARRAY_CONTAINER AllocatedClients;
	ARRAY_CONTAINER AllocatedBuffers;
	CRITICAL_SECTION csStats;

	LPFN_ACCEPTEX lpfnAcceptEx;
	LPFN_DISCONNECTEX lpfnDisconnectEx;
	LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExSockaddrs;

	ACCEPT_CONTEXT AcceptContext;
	BOOL bOutstandingAccept; // move this into it's own critical section

} WEB_SERVER_INFO, *LPWEB_SERVER_INFO;

/***************************************************************************************
 * WebAllocation.c
 ***************************************************************************************/

LPWEB_SERVER_INFO	AllocateWebServerInfo(DWORD dwNumberOfThreads);
void				DestroyWebServerInfo(LPWEB_SERVER_INFO lpServerInfo);
LPWEB_CLIENT_INFO	AllocateWebClientInfo(LPWEB_SERVER_INFO lpServerInfo, SOCKET Socket);
void				DestroyWebClientInfo(LPWEB_CLIENT_INFO lpClientInfo);
LPWEB_CLIENT_BUFFER	AllocateWebClientBuffer(LPWEB_CLIENT_INFO lpClientInfo);
LPWEB_CLIENT_BUFFER	CopyWebClientBuffer(LPWEB_CLIENT_BUFFER lpOriginalBuffer);
void				DestroyWebClientBuffer(LPWEB_CLIENT_BUFFER lpBuffer);

/***************************************************************************************
 * WebServer.c
 ***************************************************************************************/

BOOL WebServerStart(LPWEB_SERVER_INFO lpServerInfo);
void WebServerStop(LPWEB_SERVER_INFO lpServerInfo);

/***************************************************************************************
 * WebServerIO.c
 ***************************************************************************************/

BOOL WebServerPostReceive(LPWEB_CLIENT_BUFFER lpBuffer, int IOMode);
BOOL WebServerPostSend(LPWEB_CLIENT_BUFFER lpBuffer, int IOMode);
BOOL WebServerPostAccept(LPWEB_SERVER_INFO lpServerInfo);