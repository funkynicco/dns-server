#include "StdAfx.h"
#include "WebServer.h"

BOOL WebServerStart(LPWEB_SERVER_INFO lpServerInfo)
{
	if (lpServerInfo->ListenSocket != INVALID_SOCKET)
		return FALSE;

	lpServerInfo->ListenSocket = SocketPoolAllocateSocket(SOCKETTYPE_TCP);
	if (lpServerInfo->ListenSocket == INVALID_SOCKET)
		return FALSE;

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(g_Configuration.Web.NetworkInterface);
	addr.sin_port = htons(g_Configuration.Web.Port);

	if (bind(lpServerInfo->ListenSocket, (LPSOCKADDR)&addr, sizeof(addr)) == SOCKET_ERROR)
	{
		DWORD dwError = WSAGetLastError();
		SOCKETPOOL_SAFE_DESTROY(SOCKETTYPE_TCP, lpServerInfo->ListenSocket);
		WSASetLastError(dwError);
		return FALSE;
	}

	if (CreateIoCompletionPort((HANDLE)lpServerInfo->ListenSocket, lpServerInfo->IOThreads.hIocp, lpServerInfo->ListenSocket, 0) != lpServerInfo->IOThreads.hIocp)
	{
		Error(__FUNCTION__ " - Failed to associate socket to network IOCP, code: %u - %s", GetLastError(), GetErrorMessage(GetLastError()));
		DWORD dwError = GetLastError();
		SOCKETPOOL_SAFE_DESTROY(SOCKETTYPE_TCP, lpServerInfo->ListenSocket);
		SetLastError(dwError);
		return FALSE;
	}

	return TRUE;
}

void WebServerStop(LPWEB_SERVER_INFO lpServerInfo)
{
	if (lpServerInfo->ListenSocket != INVALID_SOCKET)
	{

	}
}

//

DWORD WINAPI WebServerThread(LPVOID lp)
{
	LPWEB_SERVER_INFO lpServerInfo = (LPWEB_SERVER_INFO)lp;

	LPOVERLAPPED lpOverlapped; // temp
	DWORD dwBytesTransferred;
	ULONG_PTR ulCompletionKey;

	for (;;)
	{
		if (!GetQueuedCompletionStatus(
			lpServerInfo->IOThreads.hIocp,
			&dwBytesTransferred,
			&ulCompletionKey,
			(LPOVERLAPPED*)&lpOverlapped,
			INFINITE))
		{
			DWORD dwError = GetLastError();

			continue;
		}

		if (dwBytesTransferred == 0 &&
			ulCompletionKey == 0 &&
			lpOverlapped == NULL)
			break; // the owner is killing all ServerIO threads
	}

	return 0;
}