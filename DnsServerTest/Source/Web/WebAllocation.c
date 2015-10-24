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

	lpServerInfo->lpPendingWSARecv = ArrayContainerCreate(32);
	LoggerWrite(__FUNCTION__ " - 1: %08x, 2: %08x, 3: %08x, 4: %08x",
		(ULONG_PTR)lpServerInfo->lpPendingWSARecv,
		(ULONG_PTR)lpServerInfo->lpPendingWSASend,
		(ULONG_PTR)lpServerInfo->lpAllocatedClients,
		(ULONG_PTR)lpServerInfo->lpAllocatedBuffers);
	lpServerInfo->lpPendingWSASend = ArrayContainerCreate(32);
	LoggerWrite(__FUNCTION__ " - 1: %08x, 2: %08x, 3: %08x, 4: %08x",
		(ULONG_PTR)lpServerInfo->lpPendingWSARecv,
		(ULONG_PTR)lpServerInfo->lpPendingWSASend,
		(ULONG_PTR)lpServerInfo->lpAllocatedClients,
		(ULONG_PTR)lpServerInfo->lpAllocatedBuffers);
	lpServerInfo->lpAllocatedClients = ArrayContainerCreate(16);
	LoggerWrite(__FUNCTION__ " - 1: %08x, 2: %08x, 3: %08x, 4: %08x",
		(ULONG_PTR)lpServerInfo->lpPendingWSARecv,
		(ULONG_PTR)lpServerInfo->lpPendingWSASend,
		(ULONG_PTR)lpServerInfo->lpAllocatedClients,
		(ULONG_PTR)lpServerInfo->lpAllocatedBuffers);
	lpServerInfo->lpAllocatedBuffers = ArrayContainerCreate(64);
	LoggerWrite(__FUNCTION__ " - 1: %08x, 2: %08x, 3: %08x, 4: %08x",
		(ULONG_PTR)lpServerInfo->lpPendingWSARecv,
		(ULONG_PTR)lpServerInfo->lpPendingWSASend,
		(ULONG_PTR)lpServerInfo->lpAllocatedClients,
		(ULONG_PTR)lpServerInfo->lpAllocatedBuffers);

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
	LoggerWrite(__FUNCTION__ " - Allocated %08x", lpServerInfo);
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
			Error(__FUNCTION__ " - [Warning] Forcefully terminated thread 0x%08x", (DWORD_PTR)lpServerInfo->IOThreads.hThreads[i]);
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

	ArrayContainerDestroy(lpServerInfo->lpPendingWSARecv);
	ArrayContainerDestroy(lpServerInfo->lpPendingWSASend);
	ArrayContainerDestroy(lpServerInfo->lpAllocatedClients);
	ArrayContainerDestroy(lpServerInfo->lpAllocatedBuffers);

	DeleteCriticalSection(&lpServerInfo->csStats);
	DeleteCriticalSection(&lpServerInfo->csAllocClient);
	DeleteCriticalSection(&lpServerInfo->csAllocBuffer);

	SAFE_CLOSE_SOCKET(lpServerInfo->ListenSocket);

	if (lpServerInfo->IOThreads.hIocp)
		CloseHandle(lpServerInfo->IOThreads.hIocp);

#ifdef __LOG_WEB_ALLOCATIONS
	LoggerWrite(__FUNCTION__ " - Destroyed %08x", lpServerInfo);
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
	if (!ArrayContainerAddElement(lpServerInfo->lpAllocatedClients, lpClientInfo))
		Error(__FUNCTION__ " - Failed to add lpClientInfo %08x to lpAllocatedClients", (ULONG_PTR)lpClientInfo);
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
	LoggerWrite(__FUNCTION__ " - Allocated %08x (%u allocations)", lpClientInfo, dwAllocations);
#endif // __LOG_WEB_ALLOCATIONS

	return lpClientInfo;
}

LPWEB_CLIENT_INFO CopyWebClientInfo(LPWEB_CLIENT_INFO lpOriginalClientInfo)
{
	if (IsDestroyed(lpOriginalClientInfo))
	{
		Error(__FUNCTION__ " - Copying from destroyed object %08x", (ULONG_PTR)lpOriginalClientInfo);
		return NULL;
	}

	DWORD dwAllocations;
	LPWEB_CLIENT_INFO lpClientInfo = InternalAllocateWebClientInfo(
		lpOriginalClientInfo->lpServerInfo,
		lpOriginalClientInfo->Socket,
		lpOriginalClientInfo,
		&dwAllocations);

#ifdef __LOG_WEB_ALLOCATIONS
	LoggerWrite(__FUNCTION__ " - Allocated %08x [COPY OF %08x] (%u allocations)", lpClientInfo, lpOriginalClientInfo, dwAllocations);
#endif // __LOG_WEB_ALLOCATIONS

	return lpClientInfo;
}

void DestroyWebClientInfo(LPWEB_CLIENT_INFO lpClientInfo)
{
	if (IsDestroyed(lpClientInfo))
	{
		Error(__FUNCTION__ " - DESTROYING ALREADY DESTROYED OBJECT %08x", (ULONG_PTR)lpClientInfo);
		int* a = 0;
		*a = 0;
	}

	SetDestroyed(lpClientInfo);

	LPWEB_SERVER_INFO lpServerInfo = lpClientInfo->lpServerInfo;

	EnterCriticalSection(&lpServerInfo->csAllocClient);
#ifdef __LOG_WEB_ALLOCATIONS
	LoggerWrite(__FUNCTION__ " - Destroyed %08x (%u allocations)", lpClientInfo, lpServerInfo->dwAllocatedClients - 1);
#endif // __LOG_WEB_ALLOCATIONS
	if (lpServerInfo->dwAllocatedClients == 0)
	{
		int* a = 0;
		*a = 1;
	}

	EnterCriticalSection(&lpServerInfo->csStats);
	if (!ArrayContainerDeleteElementByValue(lpServerInfo->lpAllocatedClients, lpClientInfo))
		Error(__FUNCTION__ " - Failed to remove lpClientInfo %08x from lpAllocatedClients", (ULONG_PTR)lpClientInfo);
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
	if (!ArrayContainerAddElement(lpServerInfo->lpAllocatedBuffers, lpBuffer))
		Error(__FUNCTION__ " - Failed to add lpBuffer %08x to lpAllocatedBuffers", (ULONG_PTR)lpBuffer);
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
	LoggerWrite(__FUNCTION__ " - Allocated buffer %08x (%u allocations)", lpBuffer, dwAllocations);
#endif // __LOG_WEB_ALLOCATIONS

	return lpBuffer;
}

LPWEB_CLIENT_BUFFER CopyWebClientBuffer(LPWEB_CLIENT_BUFFER lpOriginalBuffer)
{
	if (IsDestroyed(lpOriginalBuffer))
	{
		Error(__FUNCTION__ " - Copying from destroyed buffer object %08x", (ULONG_PTR)lpOriginalBuffer);
		return NULL;
	}

	DWORD dwAllocations;
	LPWEB_CLIENT_BUFFER lpBuffer = InternalAllocateWebClientBuffer(
		lpOriginalBuffer->lpClientInfo,
		lpOriginalBuffer,
		&dwAllocations);

#ifdef __LOG_WEB_ALLOCATIONS
	LoggerWrite(__FUNCTION__ " - Allocated buffer %08x [COPY OF %08x] (%u allocations)", lpBuffer, lpOriginalBuffer, dwAllocations);
#endif // __LOG_WEB_ALLOCATIONS

	return lpBuffer;
}

void DestroyWebClientBuffer(LPWEB_CLIENT_BUFFER lpBuffer)
{
	if (IsDestroyed(lpBuffer))
	{
		Error(__FUNCTION__ " - DESTROYING ALREADY DESTROYED BUFFER OBJECT %08x", (ULONG_PTR)lpBuffer);
		int* a = 0;
		*a = 0;
	}

	SetDestroyed(lpBuffer);

	LPWEB_SERVER_INFO lpServerInfo = lpBuffer->lpClientInfo->lpServerInfo;

	EnterCriticalSection(&lpServerInfo->csAllocBuffer);
#ifdef __LOG_WEB_ALLOCATIONS
	LoggerWrite(__FUNCTION__ " - Destroyed buffer %08x (%u allocations)", lpBuffer, lpServerInfo->dwAllocatedBuffers - 1);
#endif // __LOG_WEB_ALLOCATIONS
	if (lpServerInfo->dwAllocatedBuffers == 0)
	{
		int* a = 0;
		*a = 1;
	}

	EnterCriticalSection(&lpServerInfo->csStats);
	if (!ArrayContainerDeleteElementByValue(lpServerInfo->lpAllocatedBuffers, lpBuffer))
		Error(__FUNCTION__ " - Failed to remove lpBuffer %08x from lpAllocatedBuffers", (ULONG_PTR)lpBuffer);
	LeaveCriticalSection(&lpServerInfo->csStats);

	lpBuffer->next = lpServerInfo->lpFreeBuffers;
	lpServerInfo->lpFreeBuffers = lpBuffer;
	--lpServerInfo->dwAllocatedBuffers;
	LeaveCriticalSection(&lpServerInfo->csAllocBuffer);
}
#pragma endregion