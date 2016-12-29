#include "StdAfx.h"
#include "DnsServer.h"

BOOL DnsServerPostReceive(LPDNS_REQUEST_INFO lpRequestInfo, int IOMode)
{
#ifdef __LOG_DNS_SERVER_IO
	char addrtext[16];
	GetIPFromSocketAddress(&lpRequestInfo->SocketAddress, addrtext, sizeof(addrtext));
#endif // __LOG_DNS_SERVER_IO

	LPDNS_SERVER_INFO lpServerInfo = lpRequestInfo->lpServerInfo;

	WSABUF wsaBuf;
	wsaBuf.buf = lpRequestInfo->Buffer;
	wsaBuf.len = sizeof(lpRequestInfo->Buffer);

	lpRequestInfo->dwFlags = 0;
	lpRequestInfo->SockAddrLen = sizeof(lpRequestInfo->SocketAddress);
	lpRequestInfo->IOMode = IOMode;

	EnterCriticalSection(&lpServerInfo->csStats);
	ASSERT(ArrayContainerAddElement(&lpServerInfo->PendingWSARecvFrom, lpRequestInfo, NULL));
	LeaveCriticalSection(&lpServerInfo->csStats);

	int err = WSARecvFrom(
		lpRequestInfo->Socket,
		&wsaBuf,
		1,
		NULL,
		&lpRequestInfo->dwFlags,
		(LPSOCKADDR)&lpRequestInfo->SocketAddress,
		&lpRequestInfo->SockAddrLen,
		&lpRequestInfo->Overlapped,
		NULL);

	if (err == 0)
	{
		// completed directly, do we need to do anything here?
#ifdef __LOG_DNS_SERVER_IO
		LoggerWrite(
			__FUNCTION__ " - WARNING - WSARecvFrom completed directly - Socket: %u, lpRequest: %p, IOMode: %s, target: %s",
			lpRequestInfo->Socket,
			lpRequestInfo,
			GetIOMode(lpRequestInfo->IOMode),
			addrtext);
#endif // __LOG_DNS_SERVER_IO
		return TRUE;
	}

	DWORD dwError = WSAGetLastError();

	if (dwError == WSA_IO_PENDING)
	{
#ifdef __LOG_DNS_SERVER_IO
		LoggerWrite(
			__FUNCTION__ " - Posted WSARecvFrom - Socket: %u, lpRequest: %p, IOMode: %s, target: %s",
			lpRequestInfo->Socket,
			lpRequestInfo,
			GetIOMode(lpRequestInfo->IOMode),
			addrtext);
#endif // __LOG_DNS_SERVER_IO
		return TRUE;
	}

	EnterCriticalSection(&lpServerInfo->csStats);
	if (!ArrayContainerDeleteElementByValue(&lpServerInfo->PendingWSARecvFrom, lpRequestInfo))
		Error(__FUNCTION__ " - %p - %s not found in lpPendingWSARecvFrom", lpRequestInfo, GetIOMode(lpRequestInfo->IOMode));
	LeaveCriticalSection(&lpServerInfo->csStats);

#ifndef __LOG_DNS_SERVER_IO
	char addrtext[16];
	GetIPFromSocketAddress(&lpRequestInfo->SocketAddress, addrtext, sizeof(addrtext));
#endif // __LOG_DNS_SERVER_IO

	Error(__FUNCTION__ " - [ERROR] WSARecvFrom [%s] returned %d, code: %u - %s", addrtext, err, dwError, GetErrorMessage(dwError));

	return FALSE;
}

BOOL DnsServerPostSend(LPDNS_REQUEST_INFO lpRequestInfo, int IOMode)
{
#ifdef __LOG_DNS_SERVER_IO
	char addrtext[16];
	GetIPFromSocketAddress(&lpRequestInfo->SocketAddress, addrtext, sizeof(addrtext));
#endif // __LOG_DNS_SERVER_IO

	LPDNS_SERVER_INFO lpServerInfo = lpRequestInfo->lpServerInfo;

	WSABUF wsaBuf;
	wsaBuf.buf = lpRequestInfo->Buffer;
	wsaBuf.len = lpRequestInfo->dwLength;

	lpRequestInfo->IOMode = IOMode;

	EnterCriticalSection(&lpServerInfo->csStats);
	ASSERT(ArrayContainerAddElement(&lpServerInfo->PendingWSASendTo, lpRequestInfo, NULL));
	LeaveCriticalSection(&lpServerInfo->csStats);

	int err = WSASendTo(
		lpRequestInfo->Socket,
		&wsaBuf,
		1,
		NULL,
		0,
		(CONST LPSOCKADDR)&lpRequestInfo->SocketAddress,
		sizeof(lpRequestInfo->SocketAddress),
		&lpRequestInfo->Overlapped,
		NULL);

	if (err == 0)
	{
		// completed directly, do we need to do anything here?
#ifdef __LOG_DNS_SERVER_IO
		LoggerWrite(
			__FUNCTION__ " - WARNING - WSASendTo completed directly - Socket: %u, lpRequest: %p, IOMode: %s, target: %s",
			lpRequestInfo->Socket,
			lpRequestInfo,
			GetIOMode(lpRequestInfo->IOMode),
			addrtext);
#endif // __LOG_DNS_SERVER_IO
		return TRUE;
	}

	DWORD dwError = WSAGetLastError();

	if (dwError == WSA_IO_PENDING)
	{
#ifdef __LOG_DNS_SERVER_IO
		LoggerWrite(
			__FUNCTION__ " - Posted WSASendTo - Socket: %u, lpRequest: %p, IOMode: %s, target: %s",
			lpRequestInfo->Socket,
			lpRequestInfo,
			GetIOMode(lpRequestInfo->IOMode),
			addrtext);
#endif // __LOG_DNS_SERVER_IO
		return TRUE;
	}

	EnterCriticalSection(&lpServerInfo->csStats);
	if (!ArrayContainerDeleteElementByValue(&lpServerInfo->PendingWSASendTo, lpRequestInfo))
		Error(__FUNCTION__ " - %p - %s not found in lpPendingWSASendTo", lpRequestInfo, GetIOMode(lpRequestInfo->IOMode));
	LeaveCriticalSection(&lpServerInfo->csStats);

#ifndef __LOG_DNS_SERVER_IO
	char addrtext[16];
	GetIPFromSocketAddress(&lpRequestInfo->SocketAddress, addrtext, sizeof(addrtext));
#endif // __LOG_DNS_SERVER_IO

	Error(__FUNCTION__ " - [ERROR] WSASendTo [%s] returned %d, code: %u - %s", addrtext, err, dwError, GetErrorMessage(dwError));

	return FALSE;
}

DWORD WINAPI DnsServerIOHandler(LPVOID lp)
{
	LPDNS_SERVER_INFO lpServerInfo = (LPDNS_SERVER_INFO)lp;

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
			lpServerInfo->NetworkServerThreads.hIocp,
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
						__FUNCTION__ " - I/O request %p canceled, IOMode: %s, Socket: %u",
						lpRequestInfo,
						GetIOMode(lpRequestInfo->IOMode),
						lpRequestInfo->Socket);
#endif // __LOG_DNS_SERVER_IO

					switch (lpRequestInfo->IOMode)
					{
					case IO_RECV:
					case IO_RELAY_RECV:
						EnterCriticalSection(&lpServerInfo->csStats);
						if (!ArrayContainerDeleteElementByValue(&lpServerInfo->PendingWSARecvFrom, lpRequestInfo))
							Error(__FUNCTION__ " - %p - %s not found in lpPendingWSARecvFrom", lpRequestInfo, GetIOMode(lpRequestInfo->IOMode));
						LeaveCriticalSection(&lpServerInfo->csStats);
						break;
					case IO_SEND:
					case IO_RELAY_SEND:
						EnterCriticalSection(&lpServerInfo->csStats);
						if (!ArrayContainerDeleteElementByValue(&lpServerInfo->PendingWSASendTo, lpRequestInfo))
							Error(__FUNCTION__ " - %p - %s not found in lpPendingWSASendTo", lpRequestInfo, GetIOMode(lpRequestInfo->IOMode));
						LeaveCriticalSection(&lpServerInfo->csStats);
						break;
					}

					if (lpRequestInfo->lpInnerRequest)
					{
						DestroyDnsRequestInfo(lpRequestInfo->lpInnerRequest);
						lpRequestInfo->lpInnerRequest = NULL;
					}
					DestroyDnsRequestInfo(lpRequestInfo);
				}
				break;
			default:
				Error(
					__FUNCTION__ " - GetQueuedCompletionStatus returned FALSE, code: %u "
					"(dwBytesTransferred: %d, ulCompletionKey: %Id, lpRequestInfo: %p, Socket: %Id)\r\n%s",
					GetLastError(),
					dwBytesTransferred,
					ulCompletionKey,
					lpRequestInfo,
					lpRequestInfo->Socket,
					GetErrorMessage(GetLastError()));
				break;
			}

			continue;
		}

		if (dwBytesTransferred == 0 &&
			ulCompletionKey == 0 &&
			lpRequestInfo == NULL)
			break; // the owner is killing all ServerIO threads

#ifdef __LOG_DNS_SERVER_IO
		GetIPFromSocketAddress(&lpRequestInfo->SocketAddress, addrtext, sizeof(addrtext));

		LoggerWrite(
			__FUNCTION__ " - dwBytesTransferred: %u, ulCompletionKey: %u, lpRequestInfo: %p, IOMode: %s, Socket: %u [from %s]",
			dwBytesTransferred,
			ulCompletionKey,
			lpRequestInfo,
			GetIOMode(lpRequestInfo->IOMode),
			lpRequestInfo->Socket,
			addrtext);
#endif // __LOG_DNS_SERVER_IO

		switch (lpRequestInfo->IOMode)
		{
		case IO_RECV:
			EnterCriticalSection(&lpServerInfo->csStats);
			if (!ArrayContainerDeleteElementByValue(&lpServerInfo->PendingWSARecvFrom, lpRequestInfo))
				Error(__FUNCTION__ " - %p - %s not found in lpPendingWSARecvFrom", lpRequestInfo, GetIOMode(lpRequestInfo->IOMode));
			LeaveCriticalSection(&lpServerInfo->csStats);

			lpRequestInfo->dwLength = dwBytesTransferred;

#ifdef _DEBUG
			WriteBinaryPacket("IN-REQUEST", lpRequestInfo->Buffer, lpRequestInfo->dwLength);
#endif // _DEBUG

			DnsRequestHandlerPostRequest(CopyDnsRequestInfo(lpRequestInfo));

			DnsServerPostReceive(lpRequestInfo, IO_RECV);
			break;
		case IO_RELAY_RECV: // route packet to the initial client
			EnterCriticalSection(&lpServerInfo->csStats);
			if (!ArrayContainerDeleteElementByValue(&lpServerInfo->PendingWSARecvFrom, lpRequestInfo))
				Error(__FUNCTION__ " - %p - %s not found in lpPendingWSARecvFrom", lpRequestInfo, GetIOMode(lpRequestInfo->IOMode));
			LeaveCriticalSection(&lpServerInfo->csStats);

			lpRequestInfo->dwLength = dwBytesTransferred;

			if (lpRequestInfo->lpInnerRequest)
			{
				lpRequestInfo->SockAddrLen = sizeof(lpRequestInfo->SocketAddress);
				CopyMemory(&lpRequestInfo->SocketAddress, &lpRequestInfo->lpInnerRequest->SocketAddress, sizeof(SOCKADDR_IN));

#ifdef __LOG_DNS_ALLOCATIONS
				LoggerWrite(__FUNCTION__ " [IO_RELAY_RECV]- Destroyed inner %s lpRequestInfo : %p", GetIOMode(lpRequestInfo->lpInnerRequest->IOMode), lpRequestInfo->lpInnerRequest);
#endif // __LOG_DNS_ALLOCATIONS
				DestroyDnsRequestInfo(lpRequestInfo->lpInnerRequest);
				lpRequestInfo->lpInnerRequest = NULL;

                g_dnsStatistics.IncrementResolved();

				DnsServerPostSend(lpRequestInfo, IO_SEND);
			}
			else
			{
				Error(__FUNCTION__ " - (%p %s) lpInnerRequest is NULL!", lpRequestInfo, GetIOMode(lpRequestInfo->IOMode));
#ifdef __LOG_DNS_ALLOCATIONS
				LoggerWrite(__FUNCTION__ " - Destroyed %s lpRequestInfo : %p", GetIOMode(lpRequestInfo->IOMode), lpRequestInfo);
#endif // __LOG_DNS_ALLOCATIONS
				DestroyDnsRequestInfo(lpRequestInfo);
			}
			break;
		case IO_SEND:
		case IO_RELAY_SEND:
			EnterCriticalSection(&lpServerInfo->csStats);
			if (!ArrayContainerDeleteElementByValue(&lpServerInfo->PendingWSASendTo, lpRequestInfo))
				Error(__FUNCTION__ " - %p - %s not found in lpPendingWSASendTo", lpRequestInfo, GetIOMode(lpRequestInfo->IOMode));
			LeaveCriticalSection(&lpServerInfo->csStats);
			// this was a completed send request

#ifdef _DEBUG
			WriteBinaryPacket("OUT-RESPONSE", lpRequestInfo->Buffer, dwBytesTransferred);
#endif // _DEBUG

#ifdef __LOG_DNS_ALLOCATIONS
			LoggerWrite(__FUNCTION__ " - Destroyed %s lpRequestInfo : %p", GetIOMode(lpRequestInfo->IOMode), lpRequestInfo);
#endif // __LOG_DNS_ALLOCATIONS
			if (lpRequestInfo->lpInnerRequest)
			{
#ifdef __LOG_DNS_ALLOCATIONS
				LoggerWrite(__FUNCTION__ " [EXTRA]- Destroyed inner %s lpRequestInfo : %p", GetIOMode(lpRequestInfo->lpInnerRequest->IOMode), lpRequestInfo->lpInnerRequest);
#endif // __LOG_DNS_ALLOCATIONS
				DestroyDnsRequestInfo(lpRequestInfo->lpInnerRequest);
				lpRequestInfo->lpInnerRequest = NULL;
			}
			DestroyDnsRequestInfo(lpRequestInfo);
			break;
		}
	}

	return 0;
}