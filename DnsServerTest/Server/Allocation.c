#include "StdAfx.h"
#include "Server.h"

#define GetRealMemoryPointer(__obj) ((char*)(__obj) - 1)
#define IsDestroyed(__obj) (*((char*)(__obj) - 1) != 0)
#define SetDestroyed(__obj) *((char*)(__obj) - 1) = 1;
#define ClearDestroyed(__obj) *((char*)(__obj) - 1) = 0;

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

	ASSERT(InitializeCriticalSectionAndSpinCount(&lpServerInfo->csAllocRequest, 2000));
	ASSERT(InitializeCriticalSectionAndSpinCount(&lpServerInfo->csStats, 2000));

	lpServerInfo->lpPendingWSARecvFrom = ArrayContainerCreate(32);
	lpServerInfo->lpPendingWSASendTo = ArrayContainerCreate(32);
	lpServerInfo->lpAllocatedRequests = ArrayContainerCreate(64);

	if (!RequestHandlerInitialize(lpServerInfo, 8))
	{
		Error(__FUNCTION__ " - RequestHandlerInitialize returned FALSE");
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

	lpServerInfo->lpRequestTimeoutHandler = RequestTimeoutCreate();

	for (DWORD i = 0; i < lpServerInfo->dwNumberOfNetworkThreads; ++i)
	{
		ResumeThread(lpServerInfo->hNetworkThreads[i]);
	}

#ifdef __LOG_ALLOCATIONS
	LoggerWrite(__FUNCTION__ " - Allocated %08x", lpServerInfo);
#endif // __LOG_ALLOCATIONS
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
			Error(__FUNCTION__ " - [Warning] Forcefully terminated thread 0x%08x", (DWORD_PTR)lpServerInfo->hNetworkThreads[i]);
		}
		CloseHandle(lpServerInfo->hNetworkThreads[i]);
	}

	RequestHandlerShutdown(lpServerInfo);

	while (lpServerInfo->lpFreeRequests)
	{
		LPREQUEST_INFO lpRequestInfo = lpServerInfo->lpFreeRequests;
		lpServerInfo->lpFreeRequests = lpServerInfo->lpFreeRequests->next;
		free(GetRealMemoryPointer(lpRequestInfo));
	}

	ArrayContainerDestroy(lpServerInfo->lpPendingWSARecvFrom);
	ArrayContainerDestroy(lpServerInfo->lpPendingWSASendTo);
	ArrayContainerDestroy(lpServerInfo->lpAllocatedRequests);
	
	RequestTimeoutDestroy(lpServerInfo->lpRequestTimeoutHandler);

	DeleteCriticalSection(&lpServerInfo->csStats);
	DeleteCriticalSection(&lpServerInfo->csAllocRequest);

	SAFE_CLOSE_SOCKET(lpServerInfo->Socket);

	if (lpServerInfo->hNetworkIocp)
		CloseHandle(lpServerInfo->hNetworkIocp);

#ifdef __LOG_ALLOCATIONS
	LoggerWrite(__FUNCTION__ " - Destroyed %08x", lpServerInfo);
#endif // __LOG_ALLOCATIONS
	free(lpServerInfo);
}

/***************************************************************************************
 * Request Info
 ***************************************************************************************/

static LPREQUEST_INFO InternalAllocateRequestInfo(LPSERVER_INFO lpServerInfo, SOCKET Socket, LPREQUEST_INFO lpCopyOriginal)
{
	DWORD dwAllocations;
	EnterCriticalSection(&lpServerInfo->csAllocRequest);
	LPREQUEST_INFO lpRequestInfo = lpServerInfo->lpFreeRequests;
	if (lpRequestInfo)
		lpServerInfo->lpFreeRequests = lpServerInfo->lpFreeRequests->next;
	dwAllocations = ++lpServerInfo->dwAllocatedRequests;
	LeaveCriticalSection(&lpServerInfo->csAllocRequest);

	if (!lpRequestInfo) // TODO: allocate a memory block and initialize lpFreeRequests instead of allocating single instances..
		lpRequestInfo = (LPREQUEST_INFO)((char*)malloc(sizeof(REQUEST_INFO) + 1) + 1);

	EnterCriticalSection(&lpServerInfo->csStats);
	if (!ArrayContainerAddElement(lpServerInfo->lpAllocatedRequests, lpRequestInfo))
		Error(__FUNCTION__ " - Failed to add lpRequestInfo %08x to lpAllocatedRequests", (ULONG_PTR)lpRequestInfo);
	LeaveCriticalSection(&lpServerInfo->csStats);

	ClearDestroyed(lpRequestInfo);

	if (!lpCopyOriginal)
	{
		ZeroMemory(lpRequestInfo, sizeof(REQUEST_INFO));
		lpRequestInfo->lpServerInfo = lpServerInfo;
	}
	else
		CopyMemory(lpRequestInfo, lpCopyOriginal, sizeof(REQUEST_INFO));

	lpRequestInfo->Socket = Socket;

	return lpRequestInfo;
}

LPREQUEST_INFO AllocateRequestInfo(LPSERVER_INFO lpServerInfo, SOCKET Socket)
{
	LPREQUEST_INFO lpRequestInfo = InternalAllocateRequestInfo(lpServerInfo, Socket, NULL);

#ifdef __LOG_ALLOCATIONS
	LoggerWrite(__FUNCTION__ " - Allocated %08x (%u allocations)", lpRequestInfo, dwAllocations);
#endif // __LOG_ALLOCATIONS

	return lpRequestInfo;
}

LPREQUEST_INFO CopyRequestInfo(LPREQUEST_INFO lpOriginalRequestInfo)
{
	if (IsDestroyed(lpOriginalRequestInfo))
	{
		Error(__FUNCTION__ " - Copying from destroyed object %08x", (ULONG_PTR)lpOriginalRequestInfo);
		return NULL;
	}

	LPREQUEST_INFO lpRequestInfo = InternalAllocateRequestInfo(lpOriginalRequestInfo->lpServerInfo, lpOriginalRequestInfo->Socket, lpOriginalRequestInfo);

#ifdef __LOG_ALLOCATIONS
	LoggerWrite(__FUNCTION__ " - Allocated %08x [COPY OF %08x] (%u allocations)", lpRequestInfo, lpCopyOriginal, dwAllocations);
#endif // __LOG_ALLOCATIONS

	return lpRequestInfo;
}

void DestroyRequestInfo(LPREQUEST_INFO lpRequestInfo)
{
	if (IsDestroyed(lpRequestInfo))
	{
		Error(__FUNCTION__ " - DESTROYING ALREADY DESTROYED OBJECT %08x", (ULONG_PTR)lpRequestInfo);
		int* a = 0;
		*a = 0;
	}

	SetDestroyed(lpRequestInfo);

	LPSERVER_INFO lpServerInfo = lpRequestInfo->lpServerInfo;

	EnterCriticalSection(&lpServerInfo->csAllocRequest);
#ifdef __LOG_ALLOCATIONS
	LoggerWrite(__FUNCTION__ " - Destroyed %08x (%u allocations)", lpRequestInfo, lpServerInfo->dwAllocatedRequests - 1);
#endif // __LOG_ALLOCATIONS
	if (lpServerInfo->dwAllocatedRequests == 0)
	{
		int* a = 0;
		*a = 1;
	}

	EnterCriticalSection(&lpServerInfo->csStats);
	if (!ArrayContainerDeleteElementByValue(lpServerInfo->lpAllocatedRequests, lpRequestInfo))
		Error(__FUNCTION__ " - Failed to remove lpRequestInfo %08x from lpAllocatedRequests", (ULONG_PTR)lpRequestInfo);
	LeaveCriticalSection(&lpServerInfo->csStats);

	lpRequestInfo->next = lpServerInfo->lpFreeRequests;
	lpServerInfo->lpFreeRequests = lpRequestInfo;
	--lpServerInfo->dwAllocatedRequests;
	LeaveCriticalSection(&lpServerInfo->csAllocRequest);
}