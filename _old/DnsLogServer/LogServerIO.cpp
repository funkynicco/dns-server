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
		Error(__FUNCTION__ " - %p - %s not found in lpPendingWSARecvFrom", (ULONG_PTR)lpRequestInfo, GetIOMode(lpRequestInfo->IOMode));
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
		Error(__FUNCTION__ " - %p - %s not found in lpPendingWSASendTo", (ULONG_PTR)lpRequestInfo, GetIOMode(lpRequestInfo->IOMode));
	LeaveCriticalSection(&lpServerInfo->csStats);*/

	printf(__FUNCTION__ " - [ERROR] WSASendTo returned %d, code: %u\n", err, dwError);

	return FALSE;
}

DWORD LogServer::LogServerThread()
{
	DWORD dwBytesTransferred;
	ULONG_PTR ulCompletionKey;
	LPBUFFER_INFO lpBufferInfo;

	LPBUFFER_INFO lpNewBufferInfo;
	LPLOG_MESSAGE lpMessage;

	for (;;)
	{
		if (!GetQueuedCompletionStatus(m_hIocp,
			&dwBytesTransferred,
			&ulCompletionKey,
			(LPOVERLAPPED*)&lpBufferInfo,
			INFINITE))
		{
			DWORD dwError = GetLastError();

			switch (dwError)
			{
			case ERROR_OPERATION_ABORTED:
				if (lpBufferInfo)
					DestroyBufferInfo(lpBufferInfo);
				break;
			default:
				printf(
					__FUNCTION__ " - GetQueuedCompletionStatus returned FALSE, code: %u "
					"(dwBytesTransferred: %d, ulCompletionKey: %Id, lpBufferInfo: %p, Socket: %Id)\r\n%s",
					GetLastError(),
					dwBytesTransferred,
					ulCompletionKey,
					lpBufferInfo,
					lpBufferInfo ? lpBufferInfo->Socket : INVALID_SOCKET,
					GetErrorMessage(GetLastError()));
				break;
			}

			continue;
		}

		if (dwBytesTransferred == 0 &&
			ulCompletionKey == 0 &&
			lpBufferInfo == NULL)
			break; // the owner is killing all ServerIO threads

		switch (lpBufferInfo->IOMode)
		{
		case IO_RECV:
			lpBufferInfo->dwLength = dwBytesTransferred;

			if (lpBufferInfo->dwLength >= MIN_LOG_MESSAGE_SIZE)
			{
				lpMessage = (LPLOG_MESSAGE)&lpBufferInfo->Buffer[0];

				EnterCriticalSection(&m_csMessageBuffers);

				if (lpMessage->Flags & LOGFLAG_SET_LAST_ID)
					m_lLastMessageIndex = lpMessage->Id - 1;

				if (!m_pCurrentMessageBuffer)
				{
					m_pCurrentMessageBuffer = m_pFreeMessageBuffers;
					if (m_pCurrentMessageBuffer)
						m_pFreeMessageBuffers = m_pFreeMessageBuffers->next;
					else
						m_pCurrentMessageBuffer = (LPMESSAGE_BUFFER)malloc(sizeof(MESSAGE_BUFFER));

					ZeroMemory(m_pCurrentMessageBuffer, sizeof(MESSAGE_BUFFER));
					m_pCurrentMessageBuffer->LastId = m_pCurrentMessageBuffer->FirstId = m_lLastMessageIndex + 1;
				}

				if (lpMessage->Id > m_pCurrentMessageBuffer->LastId)
					m_pCurrentMessageBuffer->LastId = lpMessage->Id;

				/***************************************************************************************
				 * DestroyMessageBuffer
				 ***************************************************************************************/

#define DestroyMessageBuffer(_Buffer) \
				for (int i = 0; i <= int(_Buffer->LastId - _Buffer->FirstId); ++i) \
				{ \
					DestroyBufferInfo(_Buffer->Buffers[i]); \
				} \
				_Buffer->next = m_pFreeMessageBuffers; \
				m_pFreeMessageBuffers = _Buffer; \
				_Buffer = NULL;

				 /***************************************************************************************/

				if ((m_pCurrentMessageBuffer->LastId - m_pCurrentMessageBuffer->FirstId) <= MAX_MESSAGE_BUFFERS)
				{
					m_pCurrentMessageBuffer->Buffers[lpMessage->Id - m_pCurrentMessageBuffer->FirstId] = lpBufferInfo;

					if (IsMessageBufferValid(m_pCurrentMessageBuffer))
					{
						// process buffer
						ProcessMessageBuffer(m_pCurrentMessageBuffer);
						m_lLastMessageIndex = m_pCurrentMessageBuffer->LastId;
						DestroyMessageBuffer(m_pCurrentMessageBuffer);
					}
					else
						printf(__FUNCTION__ " - Received packets out of order. Id: %I64d\n", lpMessage->Id);
				}
				else
				{
					// too many buffers
					printf(__FUNCTION__ " - FATAL wasted %d message buffers\n", int(m_pCurrentMessageBuffer->LastId - m_pCurrentMessageBuffer->FirstId));
					DestroyMessageBuffer(m_pCurrentMessageBuffer);
				}

				LeaveCriticalSection(&m_csMessageBuffers);
			}
			else
				printf(__FUNCTION__ " - Received too little data (%u bytes) to read\n", lpBufferInfo->dwLength);

			lpNewBufferInfo = AllocateBufferInfo(lpBufferInfo->Socket);
			if (!PostReceive(lpNewBufferInfo, IO_RECV))
			{
				printf(__FUNCTION__ " - ERR POST RECV %p\n", lpNewBufferInfo);
				DestroyBufferInfo(lpNewBufferInfo);
			}
			break;

		case IO_SEND:
			DestroyBufferInfo(lpBufferInfo);
			break;
		}
	}

	return 0;
}