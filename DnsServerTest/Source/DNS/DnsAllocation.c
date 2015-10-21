#include "StdAfx.h"
#include "DnsServer.h"

/***************************************************************************************
 * Server Info
 ***************************************************************************************/

LPDNS_SERVER_INFO AllocateDnsServerInfo()
{
	LPDNS_SERVER_INFO lpServerInfo = (LPDNS_SERVER_INFO)malloc(sizeof(DNS_SERVER_INFO));
	ZeroMemory(lpServerInfo, sizeof(DNS_SERVER_INFO));

	lpServerInfo->Socket = INVALID_SOCKET;
	lpServerInfo->SecondaryDnsServerSocketAddress.sin_family = AF_INET;
	lpServerInfo->SecondaryDnsServerSocketAddress.sin_addr.s_addr = inet_addr("8.8.8.8");
	lpServerInfo->SecondaryDnsServerSocketAddress.sin_port = htons(53);

	lpServerInfo->NetworkServerThreads.hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 2);
	if (!lpServerInfo->NetworkServerThreads.hIocp)
	{
		free(lpServerInfo);
		return NULL;
	}

	ASSERT(InitializeCriticalSectionAndSpinCount(&lpServerInfo->csAllocRequest, 2000));
	ASSERT(InitializeCriticalSectionAndSpinCount(&lpServerInfo->csStats, 2000));

	lpServerInfo->lpPendingWSARecvFrom = ArrayContainerCreate(32);
	lpServerInfo->lpPendingWSASendTo = ArrayContainerCreate(32);
	lpServerInfo->lpAllocatedRequests = ArrayContainerCreate(64);

	if (!DnsRequestHandlerInitialize(lpServerInfo, 8))
	{
		Error(__FUNCTION__ " - RequestHandlerInitialize returned FALSE");
		DeleteCriticalSection(&lpServerInfo->csAllocRequest);
		free(lpServerInfo);
		return NULL;
	}

	DWORD WINAPI DnsServerIOHandler(LPVOID lp);;
	for (DWORD i = 0; i < 2; ++i)
	{
		lpServerInfo->NetworkServerThreads.hThreads[i] = CreateThread(NULL, 0, DnsServerIOHandler, lpServerInfo, CREATE_SUSPENDED, NULL);
		if (!lpServerInfo->NetworkServerThreads.hThreads[i])
		{
			DestroyDnsServerInfo(lpServerInfo);
			return NULL;
		}
		++lpServerInfo->NetworkServerThreads.dwNumberOfThreads;
	}

	lpServerInfo->lpRequestTimeoutHandler = DnsRequestTimeoutCreate();

	for (DWORD i = 0; i < lpServerInfo->NetworkServerThreads.dwNumberOfThreads; ++i)
	{
		ResumeThread(lpServerInfo->NetworkServerThreads.hThreads[i]);
	}

#ifdef __LOG_DNS_ALLOCATIONS
	LoggerWrite(__FUNCTION__ " - Allocated %08x", lpServerInfo);
#endif // __LOG_DNS_ALLOCATIONS
	return lpServerInfo;
}

void DestroyDnsServerInfo(LPDNS_SERVER_INFO lpServerInfo)
{
	DnsServerStop(lpServerInfo);

	if (lpServerInfo->NetworkServerThreads.hIocp)
	{
		for (DWORD i = 0; i < lpServerInfo->NetworkServerThreads.dwNumberOfThreads; ++i)
			PostQueuedCompletionStatus(lpServerInfo->NetworkServerThreads.hIocp, 0, 0, NULL);
	}

	for (DWORD i = 0; i < lpServerInfo->NetworkServerThreads.dwNumberOfThreads; ++i)
	{
		if (WaitForSingleObject(lpServerInfo->NetworkServerThreads.hThreads[i], 3000) == WAIT_TIMEOUT)
		{
			TerminateThread(lpServerInfo->NetworkServerThreads.hThreads[i], 0);
			Error(__FUNCTION__ " - [Warning] Forcefully terminated thread 0x%08x", (DWORD_PTR)lpServerInfo->NetworkServerThreads.hThreads[i]);
		}
		CloseHandle(lpServerInfo->NetworkServerThreads.hThreads[i]);
	}

	DnsRequestHandlerShutdown(lpServerInfo);

	while (lpServerInfo->lpFreeRequests)
	{
		LPDNS_REQUEST_INFO lpRequestInfo = lpServerInfo->lpFreeRequests;
		lpServerInfo->lpFreeRequests = lpServerInfo->lpFreeRequests->next;
		free(GetRealMemoryPointer(lpRequestInfo));
	}

	ArrayContainerDestroy(lpServerInfo->lpPendingWSARecvFrom);
	ArrayContainerDestroy(lpServerInfo->lpPendingWSASendTo);
	ArrayContainerDestroy(lpServerInfo->lpAllocatedRequests);
	
	DnsRequestTimeoutDestroy(lpServerInfo->lpRequestTimeoutHandler);

	DeleteCriticalSection(&lpServerInfo->csStats);
	DeleteCriticalSection(&lpServerInfo->csAllocRequest);

	SAFE_CLOSE_SOCKET(lpServerInfo->Socket);

	if (lpServerInfo->NetworkServerThreads.hIocp)
		CloseHandle(lpServerInfo->NetworkServerThreads.hIocp);

#ifdef __LOG_DNS_ALLOCATIONS
	LoggerWrite(__FUNCTION__ " - Destroyed %08x", lpServerInfo);
#endif // __LOG_DNS_ALLOCATIONS
	free(lpServerInfo);
}

/***************************************************************************************
 * Request Info
 ***************************************************************************************/

static LPDNS_REQUEST_INFO InternalAllocateDnsRequestInfo(LPDNS_SERVER_INFO lpServerInfo, SOCKET Socket, LPDNS_REQUEST_INFO lpCopyOriginal, LPDWORD lpdwAllocations)
{
	EnterCriticalSection(&lpServerInfo->csAllocRequest);
	LPDNS_REQUEST_INFO lpRequestInfo = lpServerInfo->lpFreeRequests;
	if (lpRequestInfo)
		lpServerInfo->lpFreeRequests = lpServerInfo->lpFreeRequests->next;
	*lpdwAllocations = ++lpServerInfo->dwAllocatedRequests;
	LeaveCriticalSection(&lpServerInfo->csAllocRequest);

	if (!lpRequestInfo) // TODO: allocate a memory block and initialize lpFreeRequests instead of allocating single instances..
		lpRequestInfo = (LPDNS_REQUEST_INFO)((char*)malloc(sizeof(DNS_REQUEST_INFO) + 1) + 1);

	EnterCriticalSection(&lpServerInfo->csStats);
	if (!ArrayContainerAddElement(lpServerInfo->lpAllocatedRequests, lpRequestInfo))
		Error(__FUNCTION__ " - Failed to add lpRequestInfo %08x to lpAllocatedRequests", (ULONG_PTR)lpRequestInfo);
	LeaveCriticalSection(&lpServerInfo->csStats);

	ClearDestroyed(lpRequestInfo);

	if (!lpCopyOriginal)
	{
		ZeroMemory(lpRequestInfo, sizeof(DNS_REQUEST_INFO));
		lpRequestInfo->lpServerInfo = lpServerInfo;
	}
	else
		CopyMemory(lpRequestInfo, lpCopyOriginal, sizeof(DNS_REQUEST_INFO));

	lpRequestInfo->Socket = Socket;

	return lpRequestInfo;
}

LPDNS_REQUEST_INFO AllocateDnsRequestInfo(LPDNS_SERVER_INFO lpServerInfo, SOCKET Socket)
{
	DWORD dwAllocations;
	LPDNS_REQUEST_INFO lpRequestInfo = InternalAllocateDnsRequestInfo(lpServerInfo, Socket, NULL, &dwAllocations);

#ifdef __LOG_DNS_ALLOCATIONS
	LoggerWrite(__FUNCTION__ " - Allocated %08x (%u allocations)", lpRequestInfo, dwAllocations);
#endif // __LOG_DNS_ALLOCATIONS

	return lpRequestInfo;
}

LPDNS_REQUEST_INFO CopyDnsRequestInfo(LPDNS_REQUEST_INFO lpOriginalRequestInfo)
{
	if (IsDestroyed(lpOriginalRequestInfo))
	{
		Error(__FUNCTION__ " - Copying from destroyed object %08x", (ULONG_PTR)lpOriginalRequestInfo);
		return NULL;
	}

	DWORD dwAllocations;
	LPDNS_REQUEST_INFO lpRequestInfo = InternalAllocateDnsRequestInfo(
		lpOriginalRequestInfo->lpServerInfo,
		lpOriginalRequestInfo->Socket,
		lpOriginalRequestInfo,
		&dwAllocations);

#ifdef __LOG_DNS_ALLOCATIONS
	LoggerWrite(__FUNCTION__ " - Allocated %08x [COPY OF %08x] (%u allocations)", lpRequestInfo, lpOriginalRequestInfo, dwAllocations);
#endif // __LOG_DNS_ALLOCATIONS

	return lpRequestInfo;
}

void DestroyDnsRequestInfo(LPDNS_REQUEST_INFO lpRequestInfo)
{
	if (IsDestroyed(lpRequestInfo))
	{
		Error(__FUNCTION__ " - DESTROYING ALREADY DESTROYED OBJECT %08x", (ULONG_PTR)lpRequestInfo);
		int* a = 0;
		*a = 0;
	}

	SetDestroyed(lpRequestInfo);

	LPDNS_SERVER_INFO lpServerInfo = lpRequestInfo->lpServerInfo;

	EnterCriticalSection(&lpServerInfo->csAllocRequest);
#ifdef __LOG_DNS_ALLOCATIONS
	LoggerWrite(__FUNCTION__ " - Destroyed %08x (%u allocations)", lpRequestInfo, lpServerInfo->dwAllocatedRequests - 1);
#endif // __LOG_DNS_ALLOCATIONS
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