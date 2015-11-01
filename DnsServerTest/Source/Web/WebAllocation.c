#include "StdAfx.h"
#include "WebServer.h"

DWORD WINAPI WebServerThread(LPVOID);

#pragma region WebServerInfo
LPWEB_SERVER_INFO AllocateWebServerInfo(DWORD dwNumberOfThreads)
{
	LPWEB_SERVER_INFO lpServerInfo = (LPWEB_SERVER_INFO)malloc(sizeof(WEB_SERVER_INFO));
	ZeroMemory(lpServerInfo, sizeof(WEB_SERVER_INFO));

	lpServerInfo->ListenSocket = INVALID_SOCKET;

	lpServerInfo->IOThreads.hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, dwNumberOfThreads);
	if (!lpServerInfo->IOThreads.hIocp)
	{
		free(lpServerInfo);
		return NULL;
	}

	ASSERT(InitializeCriticalSectionAndSpinCount(&lpServerInfo->csAllocBuffer, 2000));
	ASSERT(InitializeCriticalSectionAndSpinCount(&lpServerInfo->csAllocClient, 2000));
	ASSERT(InitializeCriticalSectionAndSpinCount(&lpServerInfo->csStats, 2000));

	ArrayContainerCreate(&lpServerInfo->PendingWSARecv, 32);
	LoggerWrite(__FUNCTION__ " - 1: %p, 2: %p, 3: %p, 4: %p",
		(ULONG_PTR)lpServerInfo->PendingWSARecv.pElem,
		(ULONG_PTR)lpServerInfo->PendingWSASend.pElem,
		(ULONG_PTR)lpServerInfo->AllocatedClients.pElem,
		(ULONG_PTR)lpServerInfo->AllocatedBuffers.pElem);
	ArrayContainerCreate(&lpServerInfo->PendingWSASend, 32);
	LoggerWrite(__FUNCTION__ " - 1: %p, 2: %p, 3: %p, 4: %p",
		(ULONG_PTR)lpServerInfo->PendingWSARecv.pElem,
		(ULONG_PTR)lpServerInfo->PendingWSASend.pElem,
		(ULONG_PTR)lpServerInfo->AllocatedClients.pElem,
		(ULONG_PTR)lpServerInfo->AllocatedBuffers.pElem);
	ArrayContainerCreate(&lpServerInfo->AllocatedClients, 16);
	LoggerWrite(__FUNCTION__ " - 1: %p, 2: %p, 3: %p, 4: %p",
		(ULONG_PTR)lpServerInfo->PendingWSARecv.pElem,
		(ULONG_PTR)lpServerInfo->PendingWSASend.pElem,
		(ULONG_PTR)lpServerInfo->AllocatedClients.pElem,
		(ULONG_PTR)lpServerInfo->AllocatedBuffers.pElem);
	ArrayContainerCreate(&lpServerInfo->AllocatedBuffers, 64);
	LoggerWrite(__FUNCTION__ " - 1: %p, 2: %p, 3: %p, 4: %p",
		(ULONG_PTR)lpServerInfo->PendingWSARecv.pElem,
		(ULONG_PTR)lpServerInfo->PendingWSASend.pElem,
		(ULONG_PTR)lpServerInfo->AllocatedClients.pElem,
		(ULONG_PTR)lpServerInfo->AllocatedBuffers.pElem);

	for (DWORD i = 0; i < dwNumberOfThreads; ++i)
	{
		lpServerInfo->IOThreads.hThreads[i] = CreateThread(NULL, 0, WebServerThread, lpServerInfo, CREATE_SUSPENDED, NULL);
		if (!lpServerInfo->IOThreads.hThreads[i])
		{
			DestroyWebServerInfo(lpServerInfo);
			return NULL;
		}

		++lpServerInfo->IOThreads.dwNumberOfThreads;
	}

	for (DWORD i = 0; i < lpServerInfo->IOThreads.dwNumberOfThreads; ++i)
	{
		ResumeThread(lpServerInfo->IOThreads.hThreads[i]);
	}

#ifdef __LOG_WEB_ALLOCATIONS
	LoggerWrite(__FUNCTION__ " - Allocated %p", lpServerInfo);
#endif // __LOG_WEB_ALLOCATIONS
	return lpServerInfo;
}

void DestroyWebServerInfo(LPWEB_SERVER_INFO lpServerInfo)
{
	WebServerStop(lpServerInfo);

	if (lpServerInfo->IOThreads.hIocp)
	{
		for (DWORD i = 0; i < lpServerInfo->IOThreads.dwNumberOfThreads; ++i)
			PostQueuedCompletionStatus(lpServerInfo->IOThreads.hIocp, 0, 0, NULL);
	}

	for (DWORD i = 0; i < lpServerInfo->IOThreads.dwNumberOfThreads; ++i)
	{
		if (WaitForSingleObject(lpServerInfo->IOThreads.hThreads[i], 3000) == WAIT_TIMEOUT)
		{
			TerminateThread(lpServerInfo->IOThreads.hThreads[i], 0);
			Error(__FUNCTION__ " - [Warning] Forcefully terminated thread 0x%p", lpServerInfo->IOThreads.hThreads[i]);
		}
		CloseHandle(lpServerInfo->IOThreads.hThreads[i]);
	}

	while (lpServerInfo->lpFreeClients)
	{
		LPWEB_CLIENT_INFO lpClientInfo = lpServerInfo->lpFreeClients;
		lpServerInfo->lpFreeClients = lpServerInfo->lpFreeClients->next;
		free(GetRealMemoryPointer(lpClientInfo));
	}

	while (lpServerInfo->lpFreeBuffers)
	{
		LPWEB_CLIENT_BUFFER lpBuffer = lpServerInfo->lpFreeBuffers;
		lpServerInfo->lpFreeBuffers = lpServerInfo->lpFreeBuffers->next;
		free(GetRealMemoryPointer(lpBuffer));
	}

	ArrayContainerDestroy(&lpServerInfo->PendingWSARecv);
	ArrayContainerDestroy(&lpServerInfo->PendingWSASend);
	ArrayContainerDestroy(&lpServerInfo->AllocatedClients);
	ArrayContainerDestroy(&lpServerInfo->AllocatedBuffers);

	DeleteCriticalSection(&lpServerInfo->csStats);
	DeleteCriticalSection(&lpServerInfo->csAllocClient);
	DeleteCriticalSection(&lpServerInfo->csAllocBuffer);

	SAFE_CLOSE_SOCKET(lpServerInfo->ListenSocket);

	if (lpServerInfo->IOThreads.hIocp)
		CloseHandle(lpServerInfo->IOThreads.hIocp);

#ifdef __LOG_WEB_ALLOCATIONS
	LoggerWrite(__FUNCTION__ " - Destroyed %p", lpServerInfo);
#endif // __LOG_WEB_ALLOCATIONS
	free(lpServerInfo);
}
#pragma endregion

#pragma region WebClientInfo
static LPWEB_CLIENT_INFO InternalAllocateWebClientInfo(LPWEB_SERVER_INFO lpServerInfo, SOCKET Socket, LPWEB_CLIENT_INFO lpCopyOriginal, LPDWORD lpdwAllocations)
{
	EnterCriticalSection(&lpServerInfo->csAllocClient);
	LPWEB_CLIENT_INFO lpClientInfo = lpServerInfo->lpFreeClients;
	if (lpClientInfo)
		lpServerInfo->lpFreeClients = lpServerInfo->lpFreeClients->next;
	*lpdwAllocations = ++lpServerInfo->dwAllocatedClients;
	LeaveCriticalSection(&lpServerInfo->csAllocClient);

	if (!lpClientInfo) // TODO: allocate a memory block and initialize lpFreeRequests instead of allocating single instances..
		lpClientInfo = (LPWEB_CLIENT_INFO)((char*)malloc(sizeof(WEB_CLIENT_INFO) + 1) + 1);

	EnterCriticalSection(&lpServerInfo->csStats);
	if (!ArrayContainerAddElement(&lpServerInfo->AllocatedClients, lpClientInfo, NULL))
		Error(__FUNCTION__ " - Failed to add lpClientInfo %p to lpAllocatedClients", lpClientInfo);
	LeaveCriticalSection(&lpServerInfo->csStats);

	ClearDestroyed(lpClientInfo);

	if (!lpCopyOriginal)
	{
		ZeroMemory(lpClientInfo, sizeof(WEB_CLIENT_INFO));
		lpClientInfo->lpServerInfo = lpServerInfo;
	}
	else
		CopyMemory(lpClientInfo, lpCopyOriginal, sizeof(WEB_CLIENT_INFO));

	lpClientInfo->Socket = Socket;

	return lpClientInfo;
}

LPWEB_CLIENT_INFO AllocateWebClientInfo(LPWEB_SERVER_INFO lpServerInfo, SOCKET Socket)
{
	DWORD dwAllocations;
	LPWEB_CLIENT_INFO lpClientInfo = InternalAllocateWebClientInfo(lpServerInfo, Socket, NULL, &dwAllocations);

#ifdef __LOG_WEB_ALLOCATIONS
	LoggerWrite(__FUNCTION__ " - Allocated %p (%u allocations)", lpClientInfo, dwAllocations);
#endif // __LOG_WEB_ALLOCATIONS

	return lpClientInfo;
}

LPWEB_CLIENT_INFO CopyWebClientInfo(LPWEB_CLIENT_INFO lpOriginalClientInfo)
{
	if (IsDestroyed(lpOriginalClientInfo))
	{
		Error(__FUNCTION__ " - Copying from destroyed object %p", lpOriginalClientInfo);
		return NULL;
	}

	DWORD dwAllocations;
	LPWEB_CLIENT_INFO lpClientInfo = InternalAllocateWebClientInfo(
		lpOriginalClientInfo->lpServerInfo,
		lpOriginalClientInfo->Socket,
		lpOriginalClientInfo,
		&dwAllocations);

#ifdef __LOG_WEB_ALLOCATIONS
	LoggerWrite(__FUNCTION__ " - Allocated %p [COPY OF %p] (%u allocations)", lpClientInfo, lpOriginalClientInfo, dwAllocations);
#endif // __LOG_WEB_ALLOCATIONS

	return lpClientInfo;
}

void DestroyWebClientInfo(LPWEB_CLIENT_INFO lpClientInfo)
{
	if (IsDestroyed(lpClientInfo))
	{
		Error(__FUNCTION__ " - DESTROYING ALREADY DESTROYED OBJECT %p", lpClientInfo);
		int* a = 0;
		*a = 0;
	}

	SetDestroyed(lpClientInfo);

	LPWEB_SERVER_INFO lpServerInfo = lpClientInfo->lpServerInfo;

	EnterCriticalSection(&lpServerInfo->csAllocClient);
#ifdef __LOG_WEB_ALLOCATIONS
	LoggerWrite(__FUNCTION__ " - Destroyed %p (%u allocations)", lpClientInfo, lpServerInfo->dwAllocatedClients - 1);
#endif // __LOG_WEB_ALLOCATIONS
	if (lpServerInfo->dwAllocatedClients == 0)
	{
		int* a = 0;
		*a = 1;
	}

	EnterCriticalSection(&lpServerInfo->csStats);
	if (!ArrayContainerDeleteElementByValue(&lpServerInfo->AllocatedClients, lpClientInfo))
		Error(__FUNCTION__ " - Failed to remove lpClientInfo %p from lpAllocatedClients", lpClientInfo);
	LeaveCriticalSection(&lpServerInfo->csStats);

	lpClientInfo->next = lpServerInfo->lpFreeClients;
	lpServerInfo->lpFreeClients = lpClientInfo;
	--lpServerInfo->dwAllocatedClients;
	LeaveCriticalSection(&lpServerInfo->csAllocClient);
}
#pragma endregion

#pragma region WebClientBuffer
static LPWEB_CLIENT_BUFFER InternalAllocateWebClientBuffer(LPWEB_CLIENT_INFO lpClientInfo, LPWEB_CLIENT_BUFFER lpCopyOriginal, LPDWORD lpdwAllocations)
{
	LPWEB_SERVER_INFO lpServerInfo = lpClientInfo->lpServerInfo;

	EnterCriticalSection(&lpServerInfo->csAllocBuffer);
	LPWEB_CLIENT_BUFFER lpBuffer = lpServerInfo->lpFreeBuffers;
	if (lpBuffer)
		lpServerInfo->lpFreeBuffers = lpServerInfo->lpFreeBuffers->next;
	*lpdwAllocations = ++lpServerInfo->dwAllocatedBuffers;
	LeaveCriticalSection(&lpServerInfo->csAllocBuffer);

	if (!lpBuffer) // TODO: allocate a memory block and initialize lpFreeRequests instead of allocating single instances..
		lpBuffer = (LPWEB_CLIENT_BUFFER)((char*)malloc(sizeof(WEB_CLIENT_BUFFER) + 1) + 1);

	EnterCriticalSection(&lpServerInfo->csStats);
	if (!ArrayContainerAddElement(&lpServerInfo->AllocatedBuffers, lpBuffer, NULL))
		Error(__FUNCTION__ " - Failed to add lpBuffer %p to lpAllocatedBuffers", lpBuffer);
	LeaveCriticalSection(&lpServerInfo->csStats);

	ClearDestroyed(lpBuffer);

	if (!lpCopyOriginal)
	{
		ZeroMemory(lpBuffer, sizeof(WEB_CLIENT_BUFFER));
		lpBuffer->lpClientInfo = lpClientInfo;
	}
	else
		CopyMemory(lpBuffer, lpCopyOriginal, sizeof(WEB_CLIENT_BUFFER));

	return lpBuffer;
}

LPWEB_CLIENT_BUFFER AllocateWebClientBuffer(LPWEB_CLIENT_INFO lpClientInfo)
{
	DWORD dwAllocations;
	LPWEB_CLIENT_BUFFER lpBuffer = InternalAllocateWebClientBuffer(lpClientInfo, NULL, &dwAllocations);

#ifdef __LOG_WEB_ALLOCATIONS
	LoggerWrite(__FUNCTION__ " - Allocated buffer %p (%u allocations)", lpBuffer, dwAllocations);
#endif // __LOG_WEB_ALLOCATIONS

	return lpBuffer;
}

LPWEB_CLIENT_BUFFER CopyWebClientBuffer(LPWEB_CLIENT_BUFFER lpOriginalBuffer)
{
	if (IsDestroyed(lpOriginalBuffer))
	{
		Error(__FUNCTION__ " - Copying from destroyed buffer object %p", lpOriginalBuffer);
		return NULL;
	}

	DWORD dwAllocations;
	LPWEB_CLIENT_BUFFER lpBuffer = InternalAllocateWebClientBuffer(
		lpOriginalBuffer->lpClientInfo,
		lpOriginalBuffer,
		&dwAllocations);

#ifdef __LOG_WEB_ALLOCATIONS
	LoggerWrite(__FUNCTION__ " - Allocated buffer %p [COPY OF %p] (%u allocations)", lpBuffer, lpOriginalBuffer, dwAllocations);
#endif // __LOG_WEB_ALLOCATIONS

	return lpBuffer;
}

void DestroyWebClientBuffer(LPWEB_CLIENT_BUFFER lpBuffer)
{
	if (IsDestroyed(lpBuffer))
	{
		Error(__FUNCTION__ " - DESTROYING ALREADY DESTROYED BUFFER OBJECT %p", lpBuffer);
		int* a = 0;
		*a = 0;
	}

	SetDestroyed(lpBuffer);

	LPWEB_SERVER_INFO lpServerInfo = lpBuffer->lpClientInfo->lpServerInfo;

	EnterCriticalSection(&lpServerInfo->csAllocBuffer);
#ifdef __LOG_WEB_ALLOCATIONS
	LoggerWrite(__FUNCTION__ " - Destroyed buffer %p (%u allocations)", lpBuffer, lpServerInfo->dwAllocatedBuffers - 1);
#endif // __LOG_WEB_ALLOCATIONS
	if (lpServerInfo->dwAllocatedBuffers == 0)
	{
		int* a = 0;
		*a = 1;
	}

	EnterCriticalSection(&lpServerInfo->csStats);
	if (!ArrayContainerDeleteElementByValue(&lpServerInfo->AllocatedBuffers, lpBuffer))
		Error(__FUNCTION__ " - Failed to remove lpBuffer %p from lpAllocatedBuffers", lpBuffer);
	LeaveCriticalSection(&lpServerInfo->csStats);

	lpBuffer->next = lpServerInfo->lpFreeBuffers;
	lpServerInfo->lpFreeBuffers = lpBuffer;
	--lpServerInfo->dwAllocatedBuffers;
	LeaveCriticalSection(&lpServerInfo->csAllocBuffer);
}
#pragma endregion