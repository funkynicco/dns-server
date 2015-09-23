#include "StdAfx.h"
#include "Server.h"

BOOL ServerPostReceive(LPNETWORK_BUFFER lpNetworkBuffer)
{
	printf(__FUNCTION__ " - %08x\n", (ULONG_PTR)lpNetworkBuffer);

	LPREQUEST_INFO lpRequestInfo = lpNetworkBuffer->lpRequestInfo;
	LPSERVER_INFO lpServerInfo = lpRequestInfo->lpServerInfo;

	WSABUF wsaBuf;
	wsaBuf.buf = lpNetworkBuffer->Buffer;
	wsaBuf.len = sizeof(lpNetworkBuffer->Buffer);

	lpNetworkBuffer->dwFlags = 0;
	lpRequestInfo->SockAddrLen = sizeof(lpRequestInfo->SocketAddress);

	int err = WSARecvFrom(
		lpServerInfo->Socket,
		&wsaBuf,
		1,
		NULL,
		&lpNetworkBuffer->dwFlags,
		(LPSOCKADDR)&lpRequestInfo->SocketAddress,
		&lpRequestInfo->SockAddrLen,
		&lpNetworkBuffer->Overlapped,
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

	printf(__FUNCTION__ " - WSARecvFrom returned %d, code: %u\n", err, dwError);

	return FALSE;
}

BOOL ServerPostSend(LPNETWORK_BUFFER lpNetworkBuffer)
{
	LPREQUEST_INFO lpRequestInfo = lpNetworkBuffer->lpRequestInfo;
	LPSERVER_INFO lpServerInfo = lpRequestInfo->lpServerInfo;

	WSABUF wsaBuf;
	wsaBuf.buf = lpNetworkBuffer->Buffer;
	wsaBuf.len = lpNetworkBuffer->dwLength;

	int err = WSASendTo(
		lpServerInfo->Socket,
		&wsaBuf,
		1,
		NULL,
		0,
		(CONST LPSOCKADDR)&lpRequestInfo->SocketAddress,
		sizeof(lpRequestInfo->SocketAddress),
		&lpNetworkBuffer->Overlapped,
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

	printf(__FUNCTION__ " - WSASendTo returned %d, code: %u\n", err, dwError);

	return FALSE;
}

DWORD WINAPI ServerReceive(LPVOID lp)
{
	LPSERVER_INFO lpServerInfo = (LPSERVER_INFO)lp;

	LPNETWORK_BUFFER lpNetworkBuffer;
	DWORD dwBytesTransferred;
	ULONG_PTR ulCompletionKey;

	for (;;)
	{
		dwBytesTransferred = 0;
		ulCompletionKey = 0;
		lpNetworkBuffer = NULL;

		if (!GetQueuedCompletionStatus(
			lpServerInfo->hNetworkIocp,
			&dwBytesTransferred,
			&ulCompletionKey,
			(LPOVERLAPPED*)&lpNetworkBuffer,
			INFINITE))
		{
			printf(__FUNCTION__ " - GetQueuedCompletionStatus returned FALSE, code: %u\n", WSAGetLastError());
			Sleep(50);
			continue;
		}

		if (dwBytesTransferred == 0 &&
			ulCompletionKey == 0 &&
			lpNetworkBuffer == NULL)
			break;

		// ...
		printf(
			__FUNCTION__ " - dwBytesTransferred: %u, ulCompletionKey: %u, lpNetworkBuffer: %08x\n",
			dwBytesTransferred,
			ulCompletionKey,
			(ULONG_PTR)lpNetworkBuffer);
		

		ServerPostReceive(lpNetworkBuffer);
	}

	return 0;
}