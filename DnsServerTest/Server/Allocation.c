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
		printf(__FUNCTION__ " - InitializeCriticalSectionAndSpinCount returned FALSE, code: %u - %s\n", GetLastError(), GetErrorMessage(GetLastError()));
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

	DWORD WINAPI ServerIOHandler(LPVOID lp);;
	for (DWORD i = 0; i < 2; ++i)
	{
		lpServerInfo->hNetworkThreads[i] = CreateThread(NULL, 0, ServerIOHandler, lpServerInfo, CREATE_SUSPENDED, NULL);
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

LPREQUEST_INFO AllocateRequestInfo(LPSERVER_INFO lpServerInfo, LPREQUEST_INFO lpCopyOriginal)
{
	EnterCriticalSection(&lpServerInfo->csAllocRequest);
	LPREQUEST_INFO lpRequestInfo = lpServerInfo->lpFreeRequests;
	if (lpRequestInfo)
		lpServerInfo->lpFreeRequests = lpServerInfo->lpFreeRequests->next;
	//++lpServerInfo->dwAllocatedRequests;
	LeaveCriticalSection(&lpServerInfo->csAllocRequest);

	InterlockedIncrement(&lpServerInfo->dwAllocatedRequests);

	if (!lpRequestInfo) // TODO: allocate a memory block and initialize lpFreeRequests instead of allocating single instances..
		lpRequestInfo = (LPREQUEST_INFO)malloc(sizeof(REQUEST_INFO));

	if (!lpCopyOriginal)
	{
		ZeroMemory(lpRequestInfo, sizeof(REQUEST_INFO));
		lpRequestInfo->lpServerInfo = lpServerInfo;
	}
	else
		CopyMemory(lpRequestInfo, lpCopyOriginal, sizeof(REQUEST_INFO));

	return lpRequestInfo;
}

void DestroyRequestInfo(LPREQUEST_INFO lpRequestInfo)
{
	LPSERVER_INFO lpServerInfo = lpRequestInfo->lpServerInfo;

	EnterCriticalSection(&lpServerInfo->csAllocRequest);
	lpRequestInfo->next = lpServerInfo->lpFreeRequests;
	lpServerInfo->lpFreeRequests = lpRequestInfo;
	//--lpServerInfo->dwAllocatedRequests;
	LeaveCriticalSection(&lpServerInfo->csAllocRequest);
	
	InterlockedDecrement(&lpServerInfo->dwAllocatedRequests);
}