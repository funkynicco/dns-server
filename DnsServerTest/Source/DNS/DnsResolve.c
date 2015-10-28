#include "StdAfx.h"
#include "DnsServer.h"

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
	IO_THREADS_INFO Threads;
	LPDNS_SERVER_INFO lpServerInfo;

} DnsResolveParameters;
#define DRP (&DnsResolveParameters)

BOOL InitializeDnsResolver(LPDNS_SERVER_INFO lpServerInfo)
{
	ZeroMemory(DRP, sizeof(DnsResolveParameters));
	DRP->lpServerInfo = lpServerInfo;

	DRP->Threads.dwNumberOfThreads = 0;
	for (DWORD i = 0; i < MAX_RESOLVE_THREADS; ++i)
	{
		DRP->Threads.hThreads[i] = CreateThread(NULL, 0, DnsResolveHandler, NULL, CREATE_SUSPENDED, NULL);
		if (!DRP->Threads.hThreads[i])
		{
			Error(__FUNCTION__ " - Could not create thread (%d of %d): %d - %s",
				i + 1,
				DRP->Threads.dwNumberOfThreads,
				GetLastError(),
				GetErrorMessage(GetLastError()));
			DestroyDnsResolver();
			return FALSE;
		}
		++DRP->Threads.dwNumberOfThreads;
	}

	DRP->Threads.hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, DRP->Threads.dwNumberOfThreads);
	if (!DRP->Threads.hIocp)
	{
		Error(__FUNCTION__ " - Could not create IO completion port: %d - %s", GetLastError(), GetErrorMessage(GetLastError()));
		DestroyDnsResolver();
		return FALSE;
	}

	for (DWORD i = 0; i < DRP->Threads.dwNumberOfThreads; ++i)
		ResumeThread(DRP->Threads.hThreads[i]);

	return TRUE;
}

void DestroyDnsResolver()
{
	if (DRP->Threads.hIocp)
	{
		for (DWORD i = 0; i < DRP->Threads.dwNumberOfThreads; ++i)
			PostQueuedCompletionStatus(DRP->Threads.hIocp, 0, 0, NULL);
	}

	for (DWORD i = 0; i < DRP->Threads.dwNumberOfThreads; ++i)
	{
		if (WaitForSingleObject(DRP->Threads.hThreads[i], 3000) == WAIT_TIMEOUT)
		{
			TerminateThread(DRP->Threads.hThreads[i], 0);
			Error(__FUNCTION__ " - [Warning] Forcefully terminated thread 0x%08x", (DWORD_PTR)DRP->Threads.hThreads[i]);
		}

		CloseHandle(DRP->Threads.hThreads[i]);
	}

	DRP->Threads.dwNumberOfThreads = 0;

	if (DRP->Threads.hIocp)
		CloseHandle(DRP->Threads.hIocp);
	DRP->Threads.hIocp = NULL;
}

void ResolveDns(LPDNS_REQUEST_INFO lpRequestInfo)
{
	LPDNS_REQUEST_INFO lpRelayRequestInfo = CopyDnsRequestInfo(lpRequestInfo);
	lpRelayRequestInfo->lpInnerRequest = lpRequestInfo; // IO_RELAY_SEND should delete this

	// generate a new socket
	lpRelayRequestInfo->Socket = SocketPoolAllocateSocket(SOCKETTYPE_UDP);
	if (lpRelayRequestInfo->Socket == INVALID_SOCKET)
	{
		Error(__FUNCTION__ " - Failed to create new socket for resolving DNS [request %08x], code: %u - %s",
			(ULONG_PTR)lpRequestInfo,
			WSAGetLastError(),
			GetErrorMessage(WSAGetLastError()));
		DestroyDnsRequestInfo(lpRelayRequestInfo->lpInnerRequest);
		DestroyDnsRequestInfo(lpRelayRequestInfo);
	}

	if (CreateIoCompletionPort((HANDLE)lpRelayRequestInfo->Socket, DRP->Threads.hIocp, 1, DRP->Threads.dwNumberOfThreads) != DRP->Threads.hIocp)
	{
		Error(__FUNCTION__ " - Failed to associate socket (%u) with IOCP [request %08x], code: %u - %s",
			lpRequestInfo->Socket,
			(ULONG_PTR)lpRequestInfo,
			GetLastError(),
			GetErrorMessage(GetLastError()));
		SOCKETPOOL_SAFE_DESTROY(SOCKETTYPE_UDP, lpRelayRequestInfo->Socket);
		DestroyDnsRequestInfo(lpRelayRequestInfo->lpInnerRequest);
		DestroyDnsRequestInfo(lpRelayRequestInfo);
	}

	// set the secondary socket address for secondary DNS server (in the chain)
	CopyMemory(&lpRelayRequestInfo->SocketAddress, &lpRelayRequestInfo->lpServerInfo->SecondaryDnsServerSocketAddress, sizeof(SOCKADDR_IN));
	lpRelayRequestInfo->SockAddrLen = sizeof(lpRelayRequestInfo->SocketAddress);

	LPDNS_REQUEST_INFO lpRelayResponseRequestInfo = CopyDnsRequestInfo(lpRelayRequestInfo);

	lpRelayRequestInfo->lpInnerRequest = NULL; // IO_RELAY_SEND must not destroy lpInnerRequest ...

	DnsServerPostSend(lpRelayRequestInfo, IO_RELAY_SEND);
	DnsRequestTimeoutSetCancelTimeout(lpRelayResponseRequestInfo->lpServerInfo->lpRequestTimeoutHandler, lpRelayResponseRequestInfo, time(NULL) + 5);
	DnsServerPostReceive(lpRelayResponseRequestInfo, IO_RELAY_RECV);
}

DWORD WINAPI DnsResolveHandler(LPVOID lp)
{
	LPDNS_REQUEST_INFO lpRequestInfo;
	DWORD dwBytesTransferred;
	ULONG_PTR ulCompletionKey;

#ifdef __LOG_DNS_SERVER_IO
	char addrtext[16];
#endif // __LOG_DNS_SERVER_IO

	for (;;)
	{
		lpRequestInfo = NULL;

		if (!GetQueuedCompletionStatus(
			DRP->Threads.hIocp,
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
#ifdef __LOG_DNS_SERVER_IO
					LoggerWrite(
						__FUNCTION__ " - I/O request %08x canceled, IOMode: %s, Socket: %u",
						(ULONG_PTR)lpRequestInfo,
						GetIOMode(lpRequestInfo->IOMode),
						lpRequestInfo->Socket);
#endif // __LOG_DNS_SERVER_IO

					switch (lpRequestInfo->IOMode)
					{
					case IO_RELAY_RECV:
						DnsRequestTimeoutRemoveRequest(DRP->lpServerInfo->lpRequestTimeoutHandler, lpRequestInfo);

						EnterCriticalSection(&DRP->lpServerInfo->csStats);
						if (!ArrayContainerDeleteElementByValue(&DRP->lpServerInfo->PendingWSARecvFrom, lpRequestInfo))
							Error(__FUNCTION__ " - %08x - %s not found in lpPendingWSARecvFrom", (ULONG_PTR)lpRequestInfo, GetIOMode(lpRequestInfo->IOMode));
						LeaveCriticalSection(&DRP->lpServerInfo->csStats);
						break;
					case IO_RELAY_SEND:
						EnterCriticalSection(&DRP->lpServerInfo->csStats);
						if (!ArrayContainerDeleteElementByValue(&DRP->lpServerInfo->PendingWSASendTo, lpRequestInfo))
							Error(__FUNCTION__ " - %08x - %s not found in lpPendingWSASendTo", (ULONG_PTR)lpRequestInfo, GetIOMode(lpRequestInfo->IOMode));
						LeaveCriticalSection(&DRP->lpServerInfo->csStats);
						break;
					}

					if (lpRequestInfo->lpInnerRequest)
					{
						DestroyDnsRequestInfo(lpRequestInfo->lpInnerRequest);
						lpRequestInfo->lpInnerRequest = NULL;
					}
					SOCKETPOOL_SAFE_DESTROY(SOCKETTYPE_UDP, lpRequestInfo->Socket);
					DestroyDnsRequestInfo(lpRequestInfo);
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

#ifdef __LOG_DNS_SERVER_IO
		GetIPFromSocketAddress(&lpRequestInfo->SocketAddress, addrtext, sizeof(addrtext));

		LoggerWrite(
			__FUNCTION__ " - dwBytesTransferred: %u, ulCompletionKey: %u, lpRequestInfo: %08x, IOMode: %s, Socket: %u [from %s]",
			dwBytesTransferred,
			ulCompletionKey,
			(ULONG_PTR)lpRequestInfo,
			GetIOMode(lpRequestInfo->IOMode),
			lpRequestInfo->Socket,
			addrtext);
#endif // __LOG_DNS_SERVER_IO

		switch (lpRequestInfo->IOMode)
		{
		case IO_RELAY_RECV:
			DnsRequestTimeoutRemoveRequest(DRP->lpServerInfo->lpRequestTimeoutHandler, lpRequestInfo);

			EnterCriticalSection(&DRP->lpServerInfo->csStats);
			if (!ArrayContainerDeleteElementByValue(&DRP->lpServerInfo->PendingWSARecvFrom, lpRequestInfo))
				Error(__FUNCTION__ " - %08x - %s not found in lpPendingWSARecvFrom", (ULONG_PTR)lpRequestInfo, GetIOMode(lpRequestInfo->IOMode));
			LeaveCriticalSection(&DRP->lpServerInfo->csStats);

			lpRequestInfo->dwLength = dwBytesTransferred;

			SOCKETPOOL_SAFE_DESTROY(SOCKETTYPE_UDP, lpRequestInfo->Socket); // dont need the socket to Google anymore

			if (lpRequestInfo->lpInnerRequest)
			{
				lpRequestInfo->SockAddrLen = sizeof(lpRequestInfo->SocketAddress);
				CopyMemory(&lpRequestInfo->SocketAddress, &lpRequestInfo->lpInnerRequest->SocketAddress, sizeof(SOCKADDR_IN));
				lpRequestInfo->Socket = lpRequestInfo->lpInnerRequest->Socket;

#ifdef __LOG_DNS_ALLOCATIONS
				LoggerWrite(__FUNCTION__ " [IO_RELAY_RECV]- Destroyed inner %s lpRequestInfo : %08x", GetIOMode(lpRequestInfo->lpInnerRequest->IOMode), (ULONG_PTR)lpRequestInfo->lpInnerRequest);
#endif // __LOG_DNS_ALLOCATIONS
				DestroyDnsRequestInfo(lpRequestInfo->lpInnerRequest);
				lpRequestInfo->lpInnerRequest = NULL;

				DnsServerPostSend(lpRequestInfo, IO_SEND); // send back to initial client
			}
			else
			{
				Error(__FUNCTION__ " - (%08x %s) lpInnerRequest is NULL!", (ULONG_PTR)lpRequestInfo, GetIOMode(lpRequestInfo->IOMode));
#ifdef __LOG_DNS_ALLOCATIONS
				LoggerWrite(__FUNCTION__ " - Destroyed %s lpRequestInfo : %08x", GetIOMode(lpRequestInfo->IOMode), (ULONG_PTR)lpRequestInfo);
#endif // __LOG_DNS_ALLOCATIONS
				DestroyDnsRequestInfo(lpRequestInfo);
			}
			break;
		case IO_RELAY_SEND:
			EnterCriticalSection(&DRP->lpServerInfo->csStats);
			if (!ArrayContainerDeleteElementByValue(&DRP->lpServerInfo->PendingWSASendTo, lpRequestInfo))
				Error(__FUNCTION__ " - %08x - %s not found in lpPendingWSASendTo", (ULONG_PTR)lpRequestInfo, GetIOMode(lpRequestInfo->IOMode));
			LeaveCriticalSection(&DRP->lpServerInfo->csStats);
#ifdef __LOG_DNS_ALLOCATIONS
			LoggerWrite(__FUNCTION__ " - Destroyed %s lpRequestInfo : %08x", GetIOMode(lpRequestInfo->IOMode), (ULONG_PTR)lpRequestInfo);
#endif // __LOG_DNS_ALLOCATIONS
			if (lpRequestInfo->lpInnerRequest)
			{
#ifdef __LOG_DNS_ALLOCATIONS
				LoggerWrite(__FUNCTION__ " [EXTRA]- Destroyed inner %s lpRequestInfo : %08x", GetIOMode(lpRequestInfo->lpInnerRequest->IOMode), (ULONG_PTR)lpRequestInfo->lpInnerRequest);
#endif // __LOG_DNS_ALLOCATIONS
				DestroyDnsRequestInfo(lpRequestInfo->lpInnerRequest);
				lpRequestInfo->lpInnerRequest = NULL;
			}
			DestroyDnsRequestInfo(lpRequestInfo);
			break;
		default:
			Error(__FUNCTION__ " - [ERROR] Unknown IOMode: %s on request %08x [Socket: %u] (request destroyed)",
				GetIOMode(lpRequestInfo->IOMode),
				(ULONG_PTR)lpRequestInfo,
				lpRequestInfo->Socket);
			if (lpRequestInfo->lpInnerRequest)
				DestroyDnsRequestInfo(lpRequestInfo->lpInnerRequest);
			DestroyDnsRequestInfo(lpRequestInfo);
			break;
		}
	}

	return 0;
}