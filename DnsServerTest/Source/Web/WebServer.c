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

	if (bind(lpServerInfo->ListenSocket, (LPSOCKADDR)&addr, sizeof(addr)) == SOCKET_ERROR ||
		listen(lpServerInfo->ListenSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		DWORD dwError = WSAGetLastError();
		SOCKETPOOL_SAFE_DESTROY(SOCKETTYPE_TCP, lpServerInfo->ListenSocket);
		WSASetLastError(dwError);
		return FALSE;
	}

	if (!lpServerInfo->lpfnAcceptEx)
	{
		// Acquire pointer for AcceptEx
		GUID GuidAcceptEx = WSAID_ACCEPTEX;
		DWORD dw;
		if (WSAIoctl(
			lpServerInfo->ListenSocket,
			SIO_GET_EXTENSION_FUNCTION_POINTER,
			&GuidAcceptEx,
			sizeof(GuidAcceptEx),
			&lpServerInfo->lpfnAcceptEx,
			sizeof(lpServerInfo->lpfnAcceptEx),
			&dw,
			NULL,
			NULL) == SOCKET_ERROR)
		{
			Error(__FUNCTION__ " - Failed to get AcceptEx function pointer, code: %u - %s", GetLastError(), GetErrorMessage(GetLastError()));
			DWORD dwError = GetLastError();
			SOCKETPOOL_SAFE_DESTROY(SOCKETTYPE_TCP, lpServerInfo->ListenSocket);
			WSASetLastError(dwError);
			return FALSE;
		}
	}

	if (!lpServerInfo->lpfnDisconnectEx)
	{
		// Acquire pointer for DisconnectEx
		GUID GuidDisconnectEx = WSAID_DISCONNECTEX;
		DWORD dw;
		if (WSAIoctl(
			lpServerInfo->ListenSocket,
			SIO_GET_EXTENSION_FUNCTION_POINTER,
			&GuidDisconnectEx,
			sizeof(GuidDisconnectEx),
			&lpServerInfo->lpfnDisconnectEx,
			sizeof(lpServerInfo->lpfnDisconnectEx),
			&dw,
			NULL,
			NULL) == SOCKET_ERROR)
		{
			Error(__FUNCTION__ " - Failed to get DisconnectEx function pointer, code: %u - %s", GetLastError(), GetErrorMessage(GetLastError()));
			DWORD dwError = GetLastError();
			SOCKETPOOL_SAFE_DESTROY(SOCKETTYPE_TCP, lpServerInfo->ListenSocket);
			WSASetLastError(dwError);
			return FALSE;
		}
	}

	if (!lpServerInfo->lpfnGetAcceptExSockaddrs)
	{
		// Acquire pointer for GetAcceptExSockaddrs
		GUID GuidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
		DWORD dw;
		if (WSAIoctl(
			lpServerInfo->ListenSocket,
			SIO_GET_EXTENSION_FUNCTION_POINTER,
			&GuidGetAcceptExSockaddrs,
			sizeof(GuidGetAcceptExSockaddrs),
			&lpServerInfo->lpfnGetAcceptExSockaddrs,
			sizeof(lpServerInfo->lpfnGetAcceptExSockaddrs),
			&dw,
			NULL,
			NULL) == SOCKET_ERROR)
		{
			Error(__FUNCTION__ " - Failed to get GetAcceptExSockaddrs function pointer, code: %u - %s", GetLastError(), GetErrorMessage(GetLastError()));
			DWORD dwError = GetLastError();
			SOCKETPOOL_SAFE_DESTROY(SOCKETTYPE_TCP, lpServerInfo->ListenSocket);
			WSASetLastError(dwError);
			return FALSE;
		}
	}

	if (CreateIoCompletionPort((HANDLE)lpServerInfo->ListenSocket, lpServerInfo->IOThreads.hIocp, lpServerInfo->ListenSocket, 0) != lpServerInfo->IOThreads.hIocp)
	{
		Error(__FUNCTION__ " - Failed to associate socket to network IOCP, code: %u - %s", GetLastError(), GetErrorMessage(GetLastError()));
		DWORD dwError = GetLastError();
		SOCKETPOOL_SAFE_DESTROY(SOCKETTYPE_TCP, lpServerInfo->ListenSocket);
		SetLastError(dwError);
		return FALSE;
	}

	if (!WebServerPostAccept(lpServerInfo))
	{
		Error(__FUNCTION__ " - Failed to post accept, code: %u - %s", GetLastError(), GetErrorMessage(GetLastError()));
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
		if (!CancelIoEx((HANDLE)lpServerInfo->ListenSocket, NULL))
		{
			DWORD dwError = GetLastError();
			Error(__FUNCTION__ " - Failed to cancel pending I/O operations, code: %u - %s", dwError, GetErrorMessage(dwError));
		}

		// loop through each client and destroy
		EnterCriticalSection(&lpServerInfo->csAllocClient);
		for (DWORD i = 0; i < lpServerInfo->AllocatedClients.dwElementNum; ++i)
		{
			LPWEB_CLIENT_INFO lpClientInfo = (LPWEB_CLIENT_INFO)lpServerInfo->AllocatedClients.pElem[i];

			if (!CancelIoEx((HANDLE)lpClientInfo->Socket, NULL))
			{
				DWORD dwError = GetLastError();
				Error(__FUNCTION__ " - Failed to cancel pending I/O operations on client socket: %u, code: %u - %s", lpClientInfo->Socket, dwError, GetErrorMessage(dwError));
			}
			else
				LoggerWrite(__FUNCTION__ " - CancelIoEx on client socket %u, NumberOfBuffers: %u", lpClientInfo->Socket, lpClientInfo->NumberOfBuffers);
		}
		LeaveCriticalSection(&lpServerInfo->csAllocClient);

		Sleep(100);

		char allocData[4096];
		char* ptrAllocData = allocData;

#define MAKE_ALLOC_DATA(__lpArray) \
		ptrAllocData = allocData; \
		ptrAllocData += sprintf(ptrAllocData, "[" #__lpArray ": %u] ", (__lpArray)->dwElementNum); \
		for (DWORD i = 0; i < (__lpArray)->dwElementNum; ++i) \
		{ \
			if (i > 0) \
				ptrAllocData += sprintf(ptrAllocData, ", "); \
\
			ptrAllocData += sprintf(ptrAllocData, "%08x", (ULONG_PTR)(__lpArray)->pElem[i]); \
		} \
		*ptrAllocData = 0;

		DWORD dwAllocatedClients;
		DWORD dwAllocatedBuffers;
		do
		{
			EnterCriticalSection(&lpServerInfo->csAllocClient);
			dwAllocatedClients = lpServerInfo->dwAllocatedClients;
			LeaveCriticalSection(&lpServerInfo->csAllocClient);

			EnterCriticalSection(&lpServerInfo->csAllocBuffer);
			dwAllocatedBuffers = lpServerInfo->dwAllocatedBuffers;
			LeaveCriticalSection(&lpServerInfo->csAllocBuffer);

			if (dwAllocatedClients > 0 ||
				dwAllocatedBuffers > 0)
			{
				Error(__FUNCTION__ " - [Warning] (Pending I/O operations) dwAllocatedClients: %u, dwAllocatedBuffers: %u",
					dwAllocatedClients,
					dwAllocatedBuffers);

				EnterCriticalSection(&lpServerInfo->csStats);
				MAKE_ALLOC_DATA(&lpServerInfo->AllocatedClients);
				Error("%s", allocData);
				MAKE_ALLOC_DATA(&lpServerInfo->AllocatedBuffers);
				Error("%s", allocData);
				MAKE_ALLOC_DATA(&lpServerInfo->PendingWSARecv);
				Error("%s", allocData);
				MAKE_ALLOC_DATA(&lpServerInfo->PendingWSASend);
				Error("%s", allocData);
				LeaveCriticalSection(&lpServerInfo->csStats);

				Sleep(3000);
			}
		}
		while (
			dwAllocatedClients > 0 ||
			dwAllocatedBuffers > 0);

		while (lpServerInfo->bOutstandingAccept)
		{
			Error(__FUNCTION__ " - WSAAccept has not returned ...");
			Sleep(3000);
		}

		SOCKETPOOL_SAFE_DESTROY(SOCKETTYPE_TCP, lpServerInfo->ListenSocket);
	}
}