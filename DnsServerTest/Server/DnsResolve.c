#include "StdAfx.h"
#include "Server.h"

/*
 * RFC 1035 - https://tools.ietf.org/pdf/rfc1035.pdf
 * Labels are max 63 characters
 * Domain names are max 255 characters
 * TTL positive value up to 2^31-1, in seconds
 * UDP datagrams are guarranteed to not be corrupt up to 512 bytes ~
 */

DWORD WINAPI DnsResolveHandler(LPVOID lp);

#define MAX_RESOLVE_THREADS 2

struct
{
	HANDLE hIocp;
	HANDLE hThreads[MAX_RESOLVE_THREADS];
	DWORD dwNumberOfThreads;
	LPSERVER_INFO lpServerInfo;

} DnsResolveParameters;
#define DRP (&DnsResolveParameters)

BOOL InitializeDnsResolver(LPSERVER_INFO lpServerInfo)
{
	ZeroMemory(DRP, sizeof(DnsResolveParameters));
	DRP->lpServerInfo = lpServerInfo;

	DRP->dwNumberOfThreads = 0;
	for (DWORD i = 0; i < MAX_RESOLVE_THREADS; ++i)
	{
		DRP->hThreads[i] = CreateThread(NULL, 0, DnsResolveHandler, NULL, CREATE_SUSPENDED, NULL);
		if (!DRP->hThreads[i])
		{
			Error(__FUNCTION__ " - Could not create thread (%d of %d): %d - %s",
				i + 1,
				DRP->dwNumberOfThreads,
				GetLastError(),
				GetErrorMessage(GetLastError()));
			DestroyDnsResolver();
			return FALSE;
		}
		++DRP->dwNumberOfThreads;
	}

	DRP->hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, DRP->dwNumberOfThreads);
	if (!DRP->hIocp)
	{
		Error(__FUNCTION__ " - Could not create IO completion port: %d - %s", GetLastError(), GetErrorMessage(GetLastError()));
		DestroyDnsResolver();
		return FALSE;
	}

	for (DWORD i = 0; i < DRP->dwNumberOfThreads; ++i)
		ResumeThread(DRP->hThreads[i]);

	return TRUE;
}

void DestroyDnsResolver()
{
	if (DRP->hIocp)
	{
		for (DWORD i = 0; i < DRP->dwNumberOfThreads; ++i)
			PostQueuedCompletionStatus(DRP->hIocp, 0, 0, NULL);
	}

	for (DWORD i = 0; i < DRP->dwNumberOfThreads; ++i)
	{
		if (WaitForSingleObject(DRP->hThreads[i], 3000) == WAIT_TIMEOUT)
		{
			TerminateThread(DRP->hThreads[i], 0);
			Error(__FUNCTION__ " - [Warning] Forcefully terminated thread 0x%08x", (DWORD_PTR)DRP->hThreads[i]);
		}

		CloseHandle(DRP->hThreads[i]);
	}

	DRP->dwNumberOfThreads = 0;

	if (DRP->hIocp)
		CloseHandle(DRP->hIocp);
	DRP->hIocp = NULL;
}

void ResolveDns(LPREQUEST_INFO lpRequestInfo)
{
	LPREQUEST_INFO lpRelayRequestInfo = CopyRequestInfo(lpRequestInfo);
	lpRelayRequestInfo->lpInnerRequest = lpRequestInfo; // IO_RELAY_SEND should delete this

	// generate a new socket
	lpRelayRequestInfo->Socket = AllocateSocket();
	if (lpRelayRequestInfo->Socket == INVALID_SOCKET)
	{
		Error(__FUNCTION__ " - Failed to create new socket for resolving DNS [request %08x], code: %u - %s",
			(ULONG_PTR)lpRequestInfo,
			WSAGetLastError(),
			GetErrorMessage(WSAGetLastError()));
		DestroyRequestInfo(lpRelayRequestInfo->lpInnerRequest);
		DestroyRequestInfo(lpRelayRequestInfo);
	}

	if (CreateIoCompletionPort((HANDLE)lpRelayRequestInfo->Socket, DRP->hIocp, 1, DRP->dwNumberOfThreads) != DRP->hIocp)
	{
		Error(__FUNCTION__ " - Failed to associate socket (%u) with IOCP [request %08x], code: %u - %s",
			lpRequestInfo->Socket,
			(ULONG_PTR)lpRequestInfo,
			GetLastError(),
			GetErrorMessage(GetLastError()));
		SOCKETPOOL_SAFE_DESTROY(lpRelayRequestInfo->Socket);
		DestroyRequestInfo(lpRelayRequestInfo->lpInnerRequest);
		DestroyRequestInfo(lpRelayRequestInfo);
	}

	lpRelayRequestInfo->SocketAddress.sin_addr.s_addr = inet_addr("8.8.8.8");
	lpRelayRequestInfo->SocketAddress.sin_port = htons(53);
	lpRelayRequestInfo->SockAddrLen = sizeof(lpRelayRequestInfo->SocketAddress);

	LPREQUEST_INFO lpRelayResponseRequestInfo = CopyRequestInfo(lpRelayRequestInfo);

	lpRelayRequestInfo->lpInnerRequest = NULL; // IO_RELAY_SEND must not destroy lpInnerRequest ...

	PostSend(lpRelayRequestInfo, IO_RELAY_SEND);
	RequestTimeoutSetCancelTimeout(lpRelayResponseRequestInfo->lpServerInfo->lpRequestTimeoutHandler, lpRelayResponseRequestInfo, time(NULL) + 5);
	PostReceive(lpRelayResponseRequestInfo, IO_RELAY_RECV);
}

DWORD WINAPI DnsResolveHandler(LPVOID lp)
{
	LPREQUEST_INFO lpRequestInfo;
	DWORD dwBytesTransferred;
	ULONG_PTR ulCompletionKey;

#ifdef __LOG_SERVER_IO
	char addrtext[16];
#endif // __LOG_SERVER_IO

	for (;;)
	{
		lpRequestInfo = NULL;

		if (!GetQueuedCompletionStatus(
			DRP->hIocp,
			&dwBytesTransferred,
			&ulCompletionKey,
			(LPOVERLAPPED*)&lpRequestInfo,
			INFINITE))
		{
			DWORD dwError = GetLastError();

			switch (dwError)
			{
			case ERROR_OPERATION_ABORTED:
				if (lpRequestInfo)
				{
#ifdef __LOG_SERVER_IO
					LoggerWrite(
						__FUNCTION__ " - I/O request %08x canceled, IOMode: %s, Socket: %u",
						(ULONG_PTR)lpRequestInfo,
						GetIOMode(lpRequestInfo->IOMode),
						lpRequestInfo->Socket);
#endif // __LOG_SERVER_IO

					switch (lpRequestInfo->IOMode)
					{
					case IO_RELAY_RECV:
						RequestTimeoutRemoveRequest(DRP->lpServerInfo->lpRequestTimeoutHandler, lpRequestInfo);

						EnterCriticalSection(&DRP->lpServerInfo->csStats);
						if (!ArrayContainerDeleteElementByValue(DRP->lpServerInfo->lpPendingWSARecvFrom, lpRequestInfo))
							Error(__FUNCTION__ " - %08x - %s not found in lpPendingWSARecvFrom", (ULONG_PTR)lpRequestInfo, GetIOMode(lpRequestInfo->IOMode));
						LeaveCriticalSection(&DRP->lpServerInfo->csStats);
						break;
					case IO_RELAY_SEND:
						EnterCriticalSection(&DRP->lpServerInfo->csStats);
						if (!ArrayContainerDeleteElementByValue(DRP->lpServerInfo->lpPendingWSASendTo, lpRequestInfo))
							Error(__FUNCTION__ " - %08x - %s not found in lpPendingWSASendTo", (ULONG_PTR)lpRequestInfo, GetIOMode(lpRequestInfo->IOMode));
						LeaveCriticalSection(&DRP->lpServerInfo->csStats);
						break;
					}

					if (lpRequestInfo->lpInnerRequest)
					{
						DestroyRequestInfo(lpRequestInfo->lpInnerRequest);
						lpRequestInfo->lpInnerRequest = NULL;
					}
					SOCKETPOOL_SAFE_DESTROY(lpRequestInfo->Socket);
					DestroyRequestInfo(lpRequestInfo);
				}
				break;
			default:
				Error(
					__FUNCTION__ " - GetQueuedCompletionStatus returned FALSE, code: %u "
					"(dwBytesTransferred: %d, ulCompletionKey: %u, lpRequestInfo: %08x, Socket: %u)\r\n%s",
					GetLastError(),
					dwBytesTransferred,
					ulCompletionKey,
					(ULONG_PTR)lpRequestInfo,
					lpRequestInfo->Socket,
					GetErrorMessage(GetLastError()));
				break;
			}

			continue;
		}

		if (dwBytesTransferred == 0 &&
			ulCompletionKey == 0 &&
			lpRequestInfo == NULL)
			break; // the owner is killing all DnsResolve threads

#ifdef __LOG_SERVER_IO
		GetIPFromSocketAddress(&lpRequestInfo->SocketAddress, addrtext, sizeof(addrtext));

		LoggerWrite(
			__FUNCTION__ " - dwBytesTransferred: %u, ulCompletionKey: %u, lpRequestInfo: %08x, IOMode: %s, Socket: %u [from %s]",
			dwBytesTransferred,
			ulCompletionKey,
			(ULONG_PTR)lpRequestInfo,
			GetIOMode(lpRequestInfo->IOMode),
			lpRequestInfo->Socket,
			addrtext);
#endif // __LOG_SERVER_IO

		switch (lpRequestInfo->IOMode)
		{
		case IO_RELAY_RECV:
			RequestTimeoutRemoveRequest(DRP->lpServerInfo->lpRequestTimeoutHandler, lpRequestInfo);

			EnterCriticalSection(&DRP->lpServerInfo->csStats);
			if (!ArrayContainerDeleteElementByValue(DRP->lpServerInfo->lpPendingWSARecvFrom, lpRequestInfo))
				Error(__FUNCTION__ " - %08x - %s not found in lpPendingWSARecvFrom", (ULONG_PTR)lpRequestInfo, GetIOMode(lpRequestInfo->IOMode));
			LeaveCriticalSection(&DRP->lpServerInfo->csStats);

			lpRequestInfo->dwLength = dwBytesTransferred;

			SOCKETPOOL_SAFE_DESTROY(lpRequestInfo->Socket); // dont need the socket to Google anymore

			if (lpRequestInfo->lpInnerRequest)
			{
				lpRequestInfo->SockAddrLen = sizeof(lpRequestInfo->SocketAddress);
				CopyMemory(&lpRequestInfo->SocketAddress, &lpRequestInfo->lpInnerRequest->SocketAddress, sizeof(SOCKADDR_IN));
				lpRequestInfo->Socket = lpRequestInfo->lpInnerRequest->Socket;

#ifdef __LOG_ALLOCATIONS
				LoggerWrite(__FUNCTION__ " [IO_RELAY_RECV]- Destroyed inner %s lpRequestInfo : %08x", GetIOMode(lpRequestInfo->lpInnerRequest->IOMode), (ULONG_PTR)lpRequestInfo->lpInnerRequest);
#endif // __LOG_ALLOCATIONS
				DestroyRequestInfo(lpRequestInfo->lpInnerRequest);
				lpRequestInfo->lpInnerRequest = NULL;

				PostSend(lpRequestInfo, IO_SEND); // send back to initial client
			}
			else
			{
				Error(__FUNCTION__ " - (%08x %s) lpInnerRequest is NULL!", (ULONG_PTR)lpRequestInfo, GetIOMode(lpRequestInfo->IOMode));
#ifdef __LOG_ALLOCATIONS
				LoggerWrite(__FUNCTION__ " - Destroyed %s lpRequestInfo : %08x", GetIOMode(lpRequestInfo->IOMode), (ULONG_PTR)lpRequestInfo);
#endif // __LOG_ALLOCATIONS
				DestroyRequestInfo(lpRequestInfo);
			}
			break;
		case IO_RELAY_SEND:
			EnterCriticalSection(&DRP->lpServerInfo->csStats);
			if (!ArrayContainerDeleteElementByValue(DRP->lpServerInfo->lpPendingWSASendTo, lpRequestInfo))
				Error(__FUNCTION__ " - %08x - %s not found in lpPendingWSASendTo", (ULONG_PTR)lpRequestInfo, GetIOMode(lpRequestInfo->IOMode));
			LeaveCriticalSection(&DRP->lpServerInfo->csStats);
#ifdef __LOG_ALLOCATIONS
			LoggerWrite(__FUNCTION__ " - Destroyed %s lpRequestInfo : %08x", GetIOMode(lpRequestInfo->IOMode), (ULONG_PTR)lpRequestInfo);
#endif // __LOG_ALLOCATIONS
			if (lpRequestInfo->lpInnerRequest)
			{
#ifdef __LOG_ALLOCATIONS
				LoggerWrite(__FUNCTION__ " [EXTRA]- Destroyed inner %s lpRequestInfo : %08x", GetIOMode(lpRequestInfo->lpInnerRequest->IOMode), (ULONG_PTR)lpRequestInfo->lpInnerRequest);
#endif // __LOG_ALLOCATIONS
				DestroyRequestInfo(lpRequestInfo->lpInnerRequest);
				lpRequestInfo->lpInnerRequest = NULL;
			}
			DestroyRequestInfo(lpRequestInfo);
			break;
		default:
			Error(__FUNCTION__ " - [ERROR] Unknown IOMode: %s on request %08x [Socket: %u] (request destroyed)",
				GetIOMode(lpRequestInfo->IOMode),
				(ULONG_PTR)lpRequestInfo,
				lpRequestInfo->Socket);
			if (lpRequestInfo->lpInnerRequest)
				DestroyRequestInfo(lpRequestInfo->lpInnerRequest);
			DestroyRequestInfo(lpRequestInfo);
			break;
		}
	}

	return 0;
}