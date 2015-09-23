#include "StdAfx.h"
#include "Server.h"

/***************************************************************************************
 * Server Info
 ***************************************************************************************/

LPSERVER_INFO AllocateServerInfo()
{
	LPSERVER_INFO lpServerInfo = (LPSERVER_INFO)malloc(sizeof(SERVER_INFO));
	ZeroMemory(lpServerInfo, sizeof(SERVER_INFO));

	lpServerInfo->Socket = INVALID_SOCKET;
	lpServerInfo->hNetworkIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 2);
	if (!lpServerInfo->hNetworkIocp)
	{
		free(lpServerInfo);
		return NULL;
	}

	if (!InitializeCriticalSectionAndSpinCount(&lpServerInfo->csAllocRequest, 2000))
	{
		printf(__FUNCTION__ " - InitializeCriticalSectionAndSpinCount returned FALSE, code: %u\n", GetLastError());
		CloseHandle(lpServerInfo->hNetworkIocp);
		free(lpServerInfo);
		return NULL;
	}

	if (!RequestHandlerInitialize(lpServerInfo, 8))
	{
		printf(__FUNCTION__ " - RequestHandlerInitialize returned FALSE\n");
		DeleteCriticalSection(&lpServerInfo->csAllocRequest);
		free(lpServerInfo);
		return NULL;
	}

	DWORD WINAPI ServerReceive(LPVOID lp);;
	for (DWORD i = 0; i < 2; ++i)
	{
		lpServerInfo->hNetworkThreads[i] = CreateThread(NULL, 0, ServerReceive, lpServerInfo, CREATE_SUSPENDED, NULL);
		if (!lpServerInfo->hNetworkThreads[i])
		{
			DestroyServerInfo(lpServerInfo);
			return NULL;
		}
		++lpServerInfo->dwNumberOfNetworkThreads;
	}

	for (DWORD i = 0; i < lpServerInfo->dwNumberOfNetworkThreads; ++i)
	{
		ResumeThread(lpServerInfo->hNetworkThreads[i]);
	}

	return lpServerInfo;
}

void DestroyServerInfo(LPSERVER_INFO lpServerInfo)
{
	StopServer(lpServerInfo);

	if (lpServerInfo->hNetworkIocp)
	{
		for (DWORD i = 0; i < lpServerInfo->dwNumberOfNetworkThreads; ++i)
			PostQueuedCompletionStatus(lpServerInfo->hNetworkIocp, 0, 0, NULL);
	}

	for (DWORD i = 0; i < lpServerInfo->dwNumberOfNetworkThreads; ++i)
	{
		if (WaitForSingleObject(lpServerInfo->hNetworkThreads[i], 3000) == WAIT_TIMEOUT)
		{
			TerminateThread(lpServerInfo->hNetworkThreads[i], 0);
			printf(__FUNCTION__ " - [Warning] Forcefully terminated thread 0x%08x\n", (DWORD_PTR)lpServerInfo->hNetworkThreads[i]);
		}
		CloseHandle(lpServerInfo->hNetworkThreads[i]);
	}

	RequestHandlerShutdown(lpServerInfo);

	while (lpServerInfo->lpFreeRequests)
	{
		LPREQUEST_INFO lpRequestInfo = lpServerInfo->lpFreeRequests;
		lpServerInfo->lpFreeRequests = lpServerInfo->lpFreeRequests->next;
		free(lpRequestInfo);
	}

	DeleteCriticalSection(&lpServerInfo->csAllocRequest);

	SAFE_CLOSE_SOCKET(lpServerInfo->Socket);

	if (lpServerInfo->hNetworkIocp)
		CloseHandle(lpServerInfo->hNetworkIocp);

	free(lpServerInfo);
}

/***************************************************************************************
 * Request Info
 ***************************************************************************************/

LPREQUEST_INFO AllocateRequestInfo(LPSERVER_INFO lpServerInfo)
{
	EnterCriticalSection(&lpServerInfo->csAllocRequest);
	LPREQUEST_INFO lpRequestInfo = lpServerInfo->lpFreeRequests;
	if (lpRequestInfo)
		lpServerInfo->lpFreeRequests = lpServerInfo->lpFreeRequests->next;
	LeaveCriticalSection(&lpServerInfo->csAllocRequest);

	if (!lpRequestInfo)
		lpRequestInfo = (LPREQUEST_INFO)malloc(sizeof(REQUEST_INFO)); // may need to do GlobalAlloc or w/e...

	ZeroMemory(lpRequestInfo, sizeof(REQUEST_INFO));

	lpRequestInfo->lpServerInfo = lpServerInfo;

	return lpRequestInfo;
}

void DestroyRequestInfo(LPREQUEST_INFO lpRequestInfo)
{
	LPSERVER_INFO lpServerInfo = lpRequestInfo->lpServerInfo;

	EnterCriticalSection(&lpServerInfo->csAllocRequest);
	lpRequestInfo->next = lpServerInfo->lpFreeRequests;
	lpServerInfo->lpFreeRequests = lpRequestInfo;
	LeaveCriticalSection(&lpServerInfo->csAllocRequest);
}

/***************************************************************************************
 * Network Buffer
 ***************************************************************************************/

LPNETWORK_BUFFER AllocateNetworkBuffer(LPREQUEST_INFO lpRequestInfo)
{
	LPSERVER_INFO lpServerInfo = lpRequestInfo->lpServerInfo;

	EnterCriticalSection(&lpServerInfo->csAllocRequest);
	LPNETWORK_BUFFER lpNetworkBuffer = lpServerInfo->lpFreeNetworkBuffers;
	if (lpNetworkBuffer)
		lpServerInfo->lpFreeNetworkBuffers = lpServerInfo->lpFreeNetworkBuffers->next;
	LeaveCriticalSection(&lpServerInfo->csAllocRequest);

	// TODO: allocate a memory block and initialize lpFreeNetworkBuffers instead of allocating single instances..
	if (!lpNetworkBuffer)
		lpNetworkBuffer = (LPNETWORK_BUFFER)VirtualAlloc(NULL, sizeof(NETWORK_BUFFER), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
//		lpNetworkBuffer = (LPNETWORK_BUFFER)malloc(sizeof(NETWORK_BUFFER));

	ZeroMemory(lpNetworkBuffer, sizeof(NETWORK_BUFFER));
	
	lpNetworkBuffer->lpRequestInfo = lpRequestInfo;

	return lpNetworkBuffer;
}

void DestroyNetworkBuffer(LPNETWORK_BUFFER lpNetworkBuffer)
{
	LPREQUEST_INFO lpRequestInfo = lpNetworkBuffer->lpRequestInfo;
	LPSERVER_INFO lpServerInfo = lpRequestInfo->lpServerInfo;

	EnterCriticalSection(&lpServerInfo->csAllocRequest);
	lpNetworkBuffer->next = lpServerInfo->lpFreeNetworkBuffers;
	lpServerInfo->lpFreeNetworkBuffers = lpNetworkBuffer;
	LeaveCriticalSection(&lpServerInfo->csAllocRequest);
}