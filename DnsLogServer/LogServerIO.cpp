#include "StdAfx.h"
#include "LogServer.h"

BOOL LogServer::PostReceive(LPBUFFER_INFO lpBufferInfo, int IOMode)
{
	WSABUF wsaBuf;
	wsaBuf.buf = lpBufferInfo->Buffer;
	wsaBuf.len = sizeof(lpBufferInfo->Buffer);

	lpBufferInfo->dwFlags = 0;
	lpBufferInfo->SockAddrLen = sizeof(lpBufferInfo->SocketAddress);
	lpBufferInfo->IOMode = IOMode;

	// add to pending WSARecvFrom

	int err = WSARecvFrom(
		lpBufferInfo->Socket,
		&wsaBuf,
		1,
		NULL,
		&lpBufferInfo->dwFlags,
		(LPSOCKADDR)&lpBufferInfo->SocketAddress,
		&lpBufferInfo->SockAddrLen,
		&lpBufferInfo->Overlapped,
		NULL);

	if (err == 0) // completed directly
		return TRUE;

	DWORD dwError = WSAGetLastError();

	if (dwError == WSA_IO_PENDING) // posted WSARecvFrom
		return TRUE;

	/*EnterCriticalSection(&lpServerInfo->csStats);
	if (!ArrayContainerDeleteElementByValue(&lpServerInfo->PendingWSARecvFrom, lpRequestInfo))
		Error(__FUNCTION__ " - %08x - %s not found in lpPendingWSARecvFrom", (ULONG_PTR)lpRequestInfo, GetIOMode(lpRequestInfo->IOMode));
	LeaveCriticalSection(&lpServerInfo->csStats);*/

	printf(__FUNCTION__ " - [ERROR] WSARecvFrom returned %d, code: %u\n", err, dwError);

	return FALSE;
}

BOOL LogServer::PostSend(LPBUFFER_INFO lpBufferInfo, int IOMode)
{
	WSABUF wsaBuf;
	wsaBuf.buf = lpBufferInfo->Buffer;
	wsaBuf.len = lpBufferInfo->dwLength;

	lpBufferInfo->IOMode = IOMode;

	// add to pending wsasendto

	int err = WSASendTo(
		lpBufferInfo->Socket,
		&wsaBuf,
		1,
		NULL,
		0,
		(CONST LPSOCKADDR)&lpBufferInfo->SocketAddress,
		sizeof(lpBufferInfo->SocketAddress),
		&lpBufferInfo->Overlapped,
		NULL);

	if (err == 0) // completed directly
		return TRUE;

	DWORD dwError = WSAGetLastError();

	if (dwError == WSA_IO_PENDING) // Posted WSASendTo
		return TRUE;

	/*EnterCriticalSection(&lpServerInfo->csStats);
	if (!ArrayContainerDeleteElementByValue(&lpServerInfo->PendingWSASendTo, lpRequestInfo))
		Error(__FUNCTION__ " - %08x - %s not found in lpPendingWSASendTo", (ULONG_PTR)lpRequestInfo, GetIOMode(lpRequestInfo->IOMode));
	LeaveCriticalSection(&lpServerInfo->csStats);*/

	printf(__FUNCTION__ " - [ERROR] WSASendTo returned %d, code: %u\n", err, dwError);

	return FALSE;
}

DWORD LogServer::LogServerThread()
{
	LPBUFFER_INFO lpBufferInfo;
	DWORD dwBytesTransferred;
	ULONG_PTR ulCompletionKey;

	printf("TODO LogServerThread code! ...\n");
	int* a = 0;
	*a = 1;

	for (;;)
	{
		if (!GetQueuedCompletionStatus(m_hIocp,
			&dwBytesTransferred,
			&ulCompletionKey,
			(LPOVERLAPPED*)&lpBufferInfo,
			INFINITE))
		{

		}
	}

	return 0;
}