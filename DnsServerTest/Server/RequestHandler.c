#include "StdAfx.h"
#include "Server.h"

DWORD WINAPI RequestHandlerThread(LPVOID);

BOOL RequestHandlerInitialize(LPSERVER_INFO lpServerInfo, DWORD dwNumberOfThreads)
{
	RequestHandlerShutdown(lpServerInfo);

	if (dwNumberOfThreads > MAX_THREADS)
		return FALSE;

	LPREQUEST_HANDLER_INFO lpRequestHandler = &lpServerInfo->RequestHandler;

	lpRequestHandler->dwNumberOfThreads = 0;

	for (DWORD i = 0; i < dwNumberOfThreads; ++i)
	{
		lpRequestHandler->hThreads[i] = CreateThread(NULL, 0, RequestHandlerThread, lpServerInfo, CREATE_SUSPENDED, NULL);
		if (!lpRequestHandler->hThreads[i])
		{
			printf(__FUNCTION__ " - Could not create thread (%d of %d): %d - %s\n", i + 1, dwNumberOfThreads, GetLastError(), GetErrorMessage(GetLastError()));
			RequestHandlerShutdown(lpServerInfo);
			return FALSE;
		}
		++lpRequestHandler->dwNumberOfThreads;
	}

	lpRequestHandler->hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, dwNumberOfThreads);
	if (!lpRequestHandler->hIocp)
	{
		printf(__FUNCTION__ " - Could not create IO completion port: %d - %s\n", GetLastError(), GetErrorMessage(GetLastError()));
		RequestHandlerShutdown(lpServerInfo);
		return FALSE;
	}

	for (DWORD i = 0; i < lpRequestHandler->dwNumberOfThreads; ++i)
		ResumeThread(lpRequestHandler->hThreads[i]);

	return TRUE;
}

void RequestHandlerShutdown(LPSERVER_INFO lpServerInfo)
{
	LPREQUEST_HANDLER_INFO lpRequestHandler = &lpServerInfo->RequestHandler;

	if (lpRequestHandler->hIocp)
	{
		for (DWORD i = 0; i < lpRequestHandler->dwNumberOfThreads; ++i)
			PostQueuedCompletionStatus(lpRequestHandler->hIocp, 0, 0, NULL);
	}

	for (DWORD i = 0; i < lpRequestHandler->dwNumberOfThreads; ++i)
	{
		if (WaitForSingleObject(lpRequestHandler->hThreads[i], 3000) == WAIT_TIMEOUT)
		{
			TerminateThread(lpRequestHandler->hThreads[i], 0);
			printf(__FUNCTION__ " - [Warning] Forcefully terminated thread 0x%08x\n", (DWORD_PTR)lpRequestHandler->hThreads[i]);
		}
		CloseHandle(lpRequestHandler->hThreads[i]);
	}
	lpRequestHandler->dwNumberOfThreads = 0;

	if (lpRequestHandler->hIocp)
		CloseHandle(lpRequestHandler->hIocp);
	lpRequestHandler->hIocp = NULL;
}

void RequestHandlerPostRequest(LPREQUEST_INFO lpRequestInfo)
{
	if (!PostQueuedCompletionStatus(lpRequestInfo->lpServerInfo->RequestHandler.hIocp, 1, 1, &lpRequestInfo->Overlapped))
	{
		printf(__FUNCTION__ " - PostQueuedCompletionStatus returned FALSE, code: %u - %s\n", GetLastError(), GetErrorMessage(GetLastError()));
		DestroyRequestInfo(lpRequestInfo);
	}
}

void RequestHandlerProcessRequest(LPREQUEST_INFO lpRequestInfo)
{
	// kek wak po le banana
	char addrtext[16];
	GetIPFromSocketAddress(&lpRequestInfo->SocketAddress, addrtext, sizeof(addrtext));
	printf(__FUNCTION__ " - [Thread %u] request from %s:%d\n", GetCurrentThreadId(), addrtext, ntohs(lpRequestInfo->SocketAddress.sin_port));

	// destroy ..
	//DestroyRequestInfo(lpRequestInfo);
	strcpy(lpRequestInfo->Buffer, "w3w lel kaka");
	lpRequestInfo->dwLength = strlen(lpRequestInfo->Buffer);
	ServerPostSend(lpRequestInfo);
}

DWORD WINAPI RequestHandlerThread(LPVOID lp)
{
	LPSERVER_INFO lpServerInfo = (LPSERVER_INFO)lp;
	LPREQUEST_HANDLER_INFO lpRequestHandler = &lpServerInfo->RequestHandler;

	LPREQUEST_INFO lpRequestInfo;
	DWORD dwBytesTransferred;
	ULONG_PTR ulCompletionKey;

	for (;;)
	{
		if (!GetQueuedCompletionStatus(lpRequestHandler->hIocp, &dwBytesTransferred, &ulCompletionKey, (LPOVERLAPPED*)&lpRequestInfo, INFINITE))
		{
			printf(__FUNCTION__ " - GetQueuedCompletionStatus returned FALSE, code: %u - %s\n", GetLastError(), GetErrorMessage(GetLastError()));
			Sleep(50);
			continue;
		}

		if (dwBytesTransferred == 0) // close thread request
			break;

		RequestHandlerProcessRequest(lpRequestInfo);
	}

	return 0;
}