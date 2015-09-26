#include "StdAfx.h"
#include "Server.h"

BOOL StartServer(LPSERVER_INFO lpServerInfo, int port)
{
	if (lpServerInfo->Socket != INVALID_SOCKET)
		return FALSE;

	lpServerInfo->Socket = SocketPoolAllocateSocket();
	if (lpServerInfo->Socket == INVALID_SOCKET)
		return FALSE;

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);

	if (bind(lpServerInfo->Socket, (LPSOCKADDR)&addr, sizeof(addr)) == SOCKET_ERROR)
	{
		DWORD dwError = WSAGetLastError();
		SOCKETPOOL_SAFE_DESTROY(lpServerInfo->Socket);
		WSASetLastError(dwError);
		return FALSE;
	}

	if (CreateIoCompletionPort((HANDLE)lpServerInfo->Socket, lpServerInfo->hNetworkIocp, lpServerInfo->Socket, 0) != lpServerInfo->hNetworkIocp)
	{
		Error(__FUNCTION__ " - Failed to associate socket to network IOCP, code: %u - %s", GetLastError(), GetErrorMessage(GetLastError()));
		DWORD dwError = GetLastError();
		SOCKETPOOL_SAFE_DESTROY(lpServerInfo->Socket);
		SetLastError(dwError);
		return FALSE;
	}

	if (!InitializeDnsResolver(lpServerInfo))
	{
		DWORD dwError = GetLastError();
		Error(__FUNCTION__ " - Failed to initialize DNS Resolver");
		SOCKETPOOL_SAFE_DESTROY(lpServerInfo->Socket);
		SetLastError(dwError);
		return FALSE;
	}

	for (DWORD i = 0; i < lpServerInfo->dwNumberOfNetworkThreads; ++i)
		PostReceive(AllocateRequestInfo(lpServerInfo, lpServerInfo->Socket), IO_RECV);

	return TRUE;
}

void StopServer(LPSERVER_INFO lpServerInfo)
{
	DestroyDnsResolver();

	if (lpServerInfo->Socket != INVALID_SOCKET)
	{
		// should make sure all I/O threads stop etc..
		if (!CancelIoEx((HANDLE)lpServerInfo->Socket, NULL))
			Error(__FUNCTION__ " - Failed to cancel pending I/O operations, code: %u - %s", GetLastError(), GetErrorMessage(GetLastError()));

		Sleep(100); // allow a tiny bit of time for canceling pending I/O operations

		char allocData[4096];
		char* ptrAllocData = allocData;

#define MAKE_ALLOC_DATA(__lpArray) \
		ptrAllocData = allocData; \
		ptrAllocData += sprintf(ptrAllocData, "[" #__lpArray ": %u] ", __lpArray->dwElementNum); \
		for (DWORD i = 0; i < __lpArray->dwElementNum; ++i) \
		{ \
			if (i > 0) \
				ptrAllocData += sprintf(ptrAllocData, ", "); \
\
			LPREQUEST_INFO lpRequestInfo = (LPREQUEST_INFO)__lpArray->Elem[i]; \
			ptrAllocData += sprintf(ptrAllocData, "%08x(%s)", (ULONG_PTR)lpRequestInfo, GetIOMode(lpRequestInfo->IOMode)); \
		} \
		*ptrAllocData = 0; \

		DWORD dwAllocatedRequests;
		do
		{
			EnterCriticalSection(&lpServerInfo->csAllocRequest);
			dwAllocatedRequests = lpServerInfo->dwAllocatedRequests;
			LeaveCriticalSection(&lpServerInfo->csAllocRequest);

			if (dwAllocatedRequests > 0)
			{
				Error(__FUNCTION__ " - [Warning] (Pending I/O operations) dwAllocatedRequests: %u", dwAllocatedRequests);

				EnterCriticalSection(&lpServerInfo->csStats);
				MAKE_ALLOC_DATA(lpServerInfo->lpAllocatedRequests);
				Error("%s", allocData);
				MAKE_ALLOC_DATA(lpServerInfo->lpPendingWSARecvFrom);
				Error("%s", allocData);
				MAKE_ALLOC_DATA(lpServerInfo->lpPendingWSASendTo);
				Error("%s", allocData);
				LeaveCriticalSection(&lpServerInfo->csStats);

				Sleep(1000);
			}
		}
		while (dwAllocatedRequests > 0);

		SOCKETPOOL_SAFE_DESTROY(lpServerInfo->Socket);
	}
}

void ServerSetSecondaryDnsServer(LPSERVER_INFO lpServerInfo, LPSOCKADDR_IN lpSockAddr)
{
	CopyMemory(&lpServerInfo->SecondaryDnsServerSocketAddress, lpSockAddr, sizeof(SOCKADDR_IN));
}