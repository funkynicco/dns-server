#include "StdAfx.h"
#include "Server.h"

BOOL ServerPostReceive(LPREQUEST_INFO lpRequestInfo)
{
	LPSERVER_INFO lpServerInfo = lpRequestInfo->lpServerInfo;

	WSABUF wsaBuf;
	wsaBuf.buf = lpRequestInfo->Buffer;
	wsaBuf.len = sizeof(lpRequestInfo->Buffer);

	lpRequestInfo->dwFlags = 0;
	lpRequestInfo->SockAddrLen = sizeof(lpRequestInfo->SocketAddress);
	lpRequestInfo->IOMode = NIO_RECV;

	int err = WSARecvFrom(
		lpServerInfo->Socket,
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
		printf(__FUNCTION__ " - WARNING - WSARecvFrom completed directly\n");
		return TRUE;
	}

	DWORD dwError = WSAGetLastError();

	if (dwError == WSA_IO_PENDING)
		return TRUE;

	printf(__FUNCTION__ " - WSARecvFrom returned %d, code: %u - %s\n", err, dwError, GetErrorMessage(dwError));

	return FALSE;
}

BOOL ServerPostSend(LPREQUEST_INFO lpRequestInfo)
{
	LPSERVER_INFO lpServerInfo = lpRequestInfo->lpServerInfo;

	WSABUF wsaBuf;
	wsaBuf.buf = lpRequestInfo->Buffer;
	wsaBuf.len = lpRequestInfo->dwLength;

	lpRequestInfo->IOMode = NIO_SEND;

	int err = WSASendTo(
		lpServerInfo->Socket,
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
		printf(__FUNCTION__ " - WARNING - WSASendTo completed directly\n");
		return TRUE;
	}

	DWORD dwError = WSAGetLastError();

	if (dwError == WSA_IO_PENDING)
		return TRUE;

	printf(__FUNCTION__ " - WSASendTo returned %d, code: %u - %s\n", err, dwError, GetErrorMessage(dwError));

	return FALSE;
}

DWORD WINAPI ServerIOHandler(LPVOID lp)
{
	LPSERVER_INFO lpServerInfo = (LPSERVER_INFO)lp;

	LPREQUEST_INFO lpRequestInfo;
	DWORD dwBytesTransferred;
	ULONG_PTR ulCompletionKey;

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
					printf(
						__FUNCTION__ " - I/O request %08x canceled, IOMode: %s\n",
						(ULONG_PTR)lpRequestInfo,
						lpRequestInfo->IOMode == NIO_RECV ? "NIO_RECV" : "NIO_SEND");
					DestroyRequestInfo(lpRequestInfo);
				}
				break;
			default:
				printf(
					__FUNCTION__ " - GetQueuedCompletionStatus returned FALSE, code: %u "
					"(dwBytesTransferred: %d, ulCompletionKey: %u, lpRequestInfo: %08x)\n%s\n",
					GetLastError(),
					dwBytesTransferred,
					ulCompletionKey,
					(ULONG_PTR)lpRequestInfo,
					GetErrorMessage(GetLastError()));
				break;
			}

			continue;
		}

		if (dwBytesTransferred == 0 &&
			ulCompletionKey == 0 &&
			lpRequestInfo == NULL)
			break;

		printf(
			__FUNCTION__ " - dwBytesTransferred: %u, ulCompletionKey: %u, lpRequestInfo: %08x, IOMode: %s\n",
			dwBytesTransferred,
			ulCompletionKey,
			(ULONG_PTR)lpRequestInfo,
			lpRequestInfo->IOMode == NIO_RECV ? "NIO_RECV" : "NIO_SEND");

		if (lpRequestInfo->IOMode == NIO_RECV)
		{
			RequestHandlerPostRequest(AllocateRequestInfo(lpServerInfo, lpRequestInfo));

			ServerPostReceive(lpRequestInfo);
		}
		else
		{
			// this was a completed send request
			printf(__FUNCTION__ " - Destroyed NIO_SEND lpRequestInfo : %08x\n", (ULONG_PTR)lpRequestInfo);
			DestroyRequestInfo(lpRequestInfo);
		}
	}

	return 0;
}