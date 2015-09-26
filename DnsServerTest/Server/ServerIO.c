#include "StdAfx.h"
#include "Server.h"

BOOL PostReceive(LPREQUEST_INFO lpRequestInfo, int IOMode)
{
#ifdef __LOG_SERVER_IO
	char addrtext[16];
	GetIPFromSocketAddress(&lpRequestInfo->SocketAddress, addrtext, sizeof(addrtext));
#endif // __LOG_SERVER_IO

	LPSERVER_INFO lpServerInfo = lpRequestInfo->lpServerInfo;

	WSABUF wsaBuf;
	wsaBuf.buf = lpRequestInfo->Buffer;
	wsaBuf.len = sizeof(lpRequestInfo->Buffer);

	lpRequestInfo->dwFlags = 0;
	lpRequestInfo->SockAddrLen = sizeof(lpRequestInfo->SocketAddress);
	lpRequestInfo->IOMode = IOMode;

	EnterCriticalSection(&lpServerInfo->csStats);
	ASSERT(ArrayContainerAddElement(lpServerInfo->lpPendingWSARecvFrom, lpRequestInfo));
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
#ifdef __LOG_SERVER_IO
		LoggerWrite(
			__FUNCTION__ " - WARNING - WSARecvFrom completed directly - Socket: %u, lpRequest: %08x, IOMode: %s, target: %s",
			lpRequestInfo->Socket,
			(ULONG_PTR)lpRequestInfo,
			GetIOMode(lpRequestInfo->IOMode),
			addrtext);
#endif // __LOG_SERVER_IO
		return TRUE;
	}

	DWORD dwError = WSAGetLastError();

	if (dwError == WSA_IO_PENDING)
	{
#ifdef __LOG_SERVER_IO
		LoggerWrite(
			__FUNCTION__ " - Posted WSARecvFrom - Socket: %u, lpRequest: %08x, IOMode: %s, target: %s",
			lpRequestInfo->Socket,
			(ULONG_PTR)lpRequestInfo,
			GetIOMode(lpRequestInfo->IOMode),
			addrtext);
#endif // __LOG_SERVER_IO
		return TRUE;
	}

	EnterCriticalSection(&lpServerInfo->csStats);
	if (!ArrayContainerDeleteElementByValue(lpServerInfo->lpPendingWSARecvFrom, lpRequestInfo))
		Error(__FUNCTION__ " - %08x - %s not found in lpPendingWSARecvFrom", (ULONG_PTR)lpRequestInfo, GetIOMode(lpRequestInfo->IOMode));
	LeaveCriticalSection(&lpServerInfo->csStats);

#ifndef __LOG_SERVER_IO
	char addrtext[16];
	GetIPFromSocketAddress(&lpRequestInfo->SocketAddress, addrtext, sizeof(addrtext));
#endif // __LOG_SERVER_IO

	Error(__FUNCTION__ " - [ERROR] WSARecvFrom [%s] returned %d, code: %u - %s", addrtext, err, dwError, GetErrorMessage(dwError));

	return FALSE;
}

BOOL PostSend(LPREQUEST_INFO lpRequestInfo, int IOMode)
{
#ifdef __LOG_SERVER_IO
	char addrtext[16];
	GetIPFromSocketAddress(&lpRequestInfo->SocketAddress, addrtext, sizeof(addrtext));
#endif // __LOG_SERVER_IO

	LPSERVER_INFO lpServerInfo = lpRequestInfo->lpServerInfo;

	WSABUF wsaBuf;
	wsaBuf.buf = lpRequestInfo->Buffer;
	wsaBuf.len = lpRequestInfo->dwLength;

	lpRequestInfo->IOMode = IOMode;

	EnterCriticalSection(&lpServerInfo->csStats);
	ASSERT(ArrayContainerAddElement(lpServerInfo->lpPendingWSASendTo, lpRequestInfo));
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
#ifdef __LOG_SERVER_IO
		LoggerWrite(
			__FUNCTION__ " - WARNING - WSASendTo completed directly - Socket: %u, lpRequest: %08x, IOMode: %s, target: %s",
			lpRequestInfo->Socket,
			(ULONG_PTR)lpRequestInfo,
			GetIOMode(lpRequestInfo->IOMode),
			addrtext);
#endif // __LOG_SERVER_IO
		return TRUE;
	}

	DWORD dwError = WSAGetLastError();

	if (dwError == WSA_IO_PENDING)
	{
#ifdef __LOG_SERVER_IO
		LoggerWrite(
			__FUNCTION__ " - Posted WSASendTo - Socket: %u, lpRequest: %08x, IOMode: %s, target: %s",
			lpRequestInfo->Socket,
			(ULONG_PTR)lpRequestInfo,
			GetIOMode(lpRequestInfo->IOMode),
			addrtext);
#endif // __LOG_SERVER_IO
		return TRUE;
	}

	EnterCriticalSection(&lpServerInfo->csStats);
	if (!ArrayContainerDeleteElementByValue(lpServerInfo->lpPendingWSASendTo, lpRequestInfo))
		Error(__FUNCTION__ " - %08x - %s not found in lpPendingWSASendTo", (ULONG_PTR)lpRequestInfo, GetIOMode(lpRequestInfo->IOMode));
	LeaveCriticalSection(&lpServerInfo->csStats);

#ifndef __LOG_SERVER_IO
	char addrtext[16];
	GetIPFromSocketAddress(&lpRequestInfo->SocketAddress, addrtext, sizeof(addrtext));
#endif // __LOG_SERVER_IO

	Error(__FUNCTION__ " - [ERROR] WSASendTo [%s] returned %d, code: %u - %s", addrtext, err, dwError, GetErrorMessage(dwError));

	return FALSE;
}

DWORD WINAPI ServerIOHandler(LPVOID lp)
{
	LPSERVER_INFO lpServerInfo = (LPSERVER_INFO)lp;

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
			lpServerInfo->hNetworkIocp,
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
					case IO_RECV:
					case IO_RELAY_RECV:
						EnterCriticalSection(&lpServerInfo->csStats);
						if (!ArrayContainerDeleteElementByValue(lpServerInfo->lpPendingWSARecvFrom, lpRequestInfo))
							Error(__FUNCTION__ " - %08x - %s not found in lpPendingWSARecvFrom", (ULONG_PTR)lpRequestInfo, GetIOMode(lpRequestInfo->IOMode));
						LeaveCriticalSection(&lpServerInfo->csStats);
						break;
					case IO_SEND:
					case IO_RELAY_SEND:
						EnterCriticalSection(&lpServerInfo->csStats);
						if (!ArrayContainerDeleteElementByValue(lpServerInfo->lpPendingWSASendTo, lpRequestInfo))
							Error(__FUNCTION__ " - %08x - %s not found in lpPendingWSASendTo", (ULONG_PTR)lpRequestInfo, GetIOMode(lpRequestInfo->IOMode));
						LeaveCriticalSection(&lpServerInfo->csStats);
						break;
					}

					if (lpRequestInfo->lpInnerRequest)
					{
						DestroyRequestInfo(lpRequestInfo->lpInnerRequest);
						lpRequestInfo->lpInnerRequest = NULL;
					}
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
			break; // the owner is killing all ServerIO threads

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
		case IO_RECV:
			EnterCriticalSection(&lpServerInfo->csStats);
			if (!ArrayContainerDeleteElementByValue(lpServerInfo->lpPendingWSARecvFrom, lpRequestInfo))
				Error(__FUNCTION__ " - %08x - %s not found in lpPendingWSARecvFrom", (ULONG_PTR)lpRequestInfo, GetIOMode(lpRequestInfo->IOMode));
			LeaveCriticalSection(&lpServerInfo->csStats);

			lpRequestInfo->dwLength = dwBytesTransferred;
			RequestHandlerPostRequest(CopyRequestInfo(lpRequestInfo));

			PostReceive(lpRequestInfo, IO_RECV);
			break;
		case IO_RELAY_RECV: // route packet to the initial client
			EnterCriticalSection(&lpServerInfo->csStats);
			if (!ArrayContainerDeleteElementByValue(lpServerInfo->lpPendingWSARecvFrom, lpRequestInfo))
				Error(__FUNCTION__ " - %08x - %s not found in lpPendingWSARecvFrom", (ULONG_PTR)lpRequestInfo, GetIOMode(lpRequestInfo->IOMode));
			LeaveCriticalSection(&lpServerInfo->csStats);

			lpRequestInfo->dwLength = dwBytesTransferred;

			if (lpRequestInfo->lpInnerRequest)
			{
				lpRequestInfo->SockAddrLen = sizeof(lpRequestInfo->SocketAddress);
				CopyMemory(&lpRequestInfo->SocketAddress, &lpRequestInfo->lpInnerRequest->SocketAddress, sizeof(SOCKADDR_IN));

#ifdef __LOG_ALLOCATIONS
				LoggerWrite(__FUNCTION__ " [IO_RELAY_RECV]- Destroyed inner %s lpRequestInfo : %08x", GetIOMode(lpRequestInfo->lpInnerRequest->IOMode), (ULONG_PTR)lpRequestInfo->lpInnerRequest);
#endif // __LOG_ALLOCATIONS
				DestroyRequestInfo(lpRequestInfo->lpInnerRequest);
				lpRequestInfo->lpInnerRequest = NULL;

				PostSend(lpRequestInfo, IO_SEND);
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
		case IO_SEND:
		case IO_RELAY_SEND:
			EnterCriticalSection(&lpServerInfo->csStats);
			if (!ArrayContainerDeleteElementByValue(lpServerInfo->lpPendingWSASendTo, lpRequestInfo))
				Error(__FUNCTION__ " - %08x - %s not found in lpPendingWSASendTo", (ULONG_PTR)lpRequestInfo, GetIOMode(lpRequestInfo->IOMode));
			LeaveCriticalSection(&lpServerInfo->csStats);
			// this was a completed send request
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
		}
	}

	return 0;
}