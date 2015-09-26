#include "StdAfx.h"
#include "Server.h"

#define MAX_MEMORY_BLOCKS	256
#define OBJECTS_PER_BLOCK	256

struct LinkedListObject
{
	LPREQUEST_INFO lpRequestInfo;
	time_t tmTimeout;

	struct LinkedListObject* prev;
	struct LinkedListObject* next;
};

typedef struct _tagRequestTimeoutHandler
{
	void* MemBlocks[MAX_MEMORY_BLOCKS];
	DWORD dwMemBlocks;
	CRITICAL_SECTION csLock;

	struct LinkedListObject* lpFreeRequests;
	struct LinkedListObject* lpAllocatedRequests;

} REQUEST_TIMEOUT_HANDLER, *LPREQUEST_TIMEOUT_HANDLER;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

LPREQUEST_TIMEOUT_HANDLER RequestTimeoutCreate()
{
	LPREQUEST_TIMEOUT_HANDLER lpHandler = (LPREQUEST_TIMEOUT_HANDLER)malloc(sizeof(REQUEST_TIMEOUT_HANDLER));
	ZeroMemory(lpHandler, sizeof(REQUEST_TIMEOUT_HANDLER));

	InitializeCriticalSection(&lpHandler->csLock);

	return lpHandler;
}

void RequestTimeoutDestroy(LPREQUEST_TIMEOUT_HANDLER lpHandler)
{
	for (DWORD i = 0; i < lpHandler->dwMemBlocks; ++i)
		VirtualFree(lpHandler->MemBlocks[i], 0, MEM_RELEASE);

	DeleteCriticalSection(&lpHandler->csLock);
}

static BOOL InternalRequestTimeoutCreateBlock(LPREQUEST_TIMEOUT_HANDLER lpHandler)
{
	if (lpHandler->dwMemBlocks == MAX_MEMORY_BLOCKS)
	{
		Error(__FUNCTION__ " - [ERROR] No more memory available.");
		return FALSE;
	}

	lpHandler->MemBlocks[lpHandler->dwMemBlocks] = VirtualAlloc(
		NULL,
		sizeof(struct LinkedListObject) * OBJECTS_PER_BLOCK,
		MEM_RESERVE | MEM_COMMIT,
		PAGE_READWRITE);

	if (!lpHandler->MemBlocks[lpHandler->dwMemBlocks])
	{
		Error(__FUNCTION__ " - VirtualAlloc failed with code %d - %s", GetLastError(), GetErrorMessage(GetLastError()));
		return FALSE;
	}

	struct LinkedListObject* lpObj = (struct LinkedListObject*)lpHandler->MemBlocks[lpHandler->dwMemBlocks];
	for (int i = 0; i < OBJECTS_PER_BLOCK; ++i)
	{
		lpObj->lpRequestInfo = NULL;
		lpObj->tmTimeout = 0;
		lpObj->prev = NULL;
		lpObj->next = lpHandler->lpFreeRequests;
		lpHandler->lpFreeRequests = lpObj;
		++lpObj;
	}

	++lpHandler->dwMemBlocks;
	return TRUE;
}

void RequestTimeoutSetCancelTimeout(LPREQUEST_TIMEOUT_HANDLER lpHandler, LPREQUEST_INFO lpRequestInfo, time_t tmTimeout)
{
	EnterCriticalSection(&lpHandler->csLock);

	// TODO: check if the lpRequestInfo isn't already in the list...
	struct LinkedListObject* lpObj;
	for (lpObj = lpHandler->lpAllocatedRequests; lpObj; lpObj = lpObj->next)
	{
		if (lpObj->lpRequestInfo == lpRequestInfo)
		{
			Error(__FUNCTION__ " - lpRequestInfo %08x already found in lpAllocatedRequests", (ULONG_PTR)lpRequestInfo);
			break;
		}
	}

	if (!lpObj)
	{
		if (!lpHandler->lpFreeRequests &&
			!InternalRequestTimeoutCreateBlock(lpHandler))
		{
			LeaveCriticalSection(&lpHandler->csLock);
			Error(__FUNCTION__ " - Failed to allocate more blocks for lpRequestInfo: %08x", (ULONG_PTR)lpRequestInfo);
			return;
		}

		// remove first element in lpFreeRequests
		lpObj = lpHandler->lpFreeRequests;
		lpHandler->lpFreeRequests = lpHandler->lpFreeRequests->next;
	}

	lpObj->lpRequestInfo = lpRequestInfo;
	lpObj->tmTimeout = tmTimeout;

	// insert into lpAllocatedRequests
	lpObj->prev = NULL;
	lpObj->next = lpHandler->lpAllocatedRequests;
	if (lpHandler->lpAllocatedRequests)
		lpHandler->lpAllocatedRequests->prev = lpObj;
	lpHandler->lpAllocatedRequests = lpObj;

	LeaveCriticalSection(&lpHandler->csLock);
}

static InternalRequestTimeoutRemoveObject(LPREQUEST_TIMEOUT_HANDLER lpHandler, struct LinkedListObject* lpObj)
{
	if (lpObj->next)
		lpObj->next->prev = lpObj->prev;
	if (lpObj->prev)
		lpObj->prev->next = lpObj->next;
	if (lpObj == lpHandler->lpAllocatedRequests)
		lpHandler->lpAllocatedRequests = lpHandler->lpAllocatedRequests->next;
}

void RequestTimeoutRemoveRequest(LPREQUEST_TIMEOUT_HANDLER lpHandler, LPREQUEST_INFO lpRequestInfo)
{
	EnterCriticalSection(&lpHandler->csLock);

	struct LinkedListObject* lpObj;
	for (lpObj = lpHandler->lpAllocatedRequests; lpObj; lpObj = lpObj->next)
	{
		if (lpObj->lpRequestInfo == lpRequestInfo)
		{
			InternalRequestTimeoutRemoveObject(lpHandler, lpObj);
			break;
		}
	}

	LeaveCriticalSection(&lpHandler->csLock);
}

void RequestTimeoutProcess(LPREQUEST_TIMEOUT_HANDLER lpHandler)
{
	EnterCriticalSection(&lpHandler->csLock);

	time_t tmNow = time(NULL);

	struct LinkedListObject* lpObj = lpHandler->lpAllocatedRequests;
	while (lpObj)
	{
		if (tmNow >= lpObj->tmTimeout)
		{
			if (!CancelIoEx((HANDLE)lpObj->lpRequestInfo->Socket, &lpObj->lpRequestInfo->Overlapped))
			{
				Error(__FUNCTION__ " - CancelIoEx [lpRequestInfo: %08x, Socket: %u] failed with code %u - %s",
					(ULONG_PTR)lpObj->lpRequestInfo,
					lpObj->lpRequestInfo->Socket,
					GetLastError(),
					GetErrorMessage(GetLastError()));
			}
			else
				Error(__FUNCTION__ " - %08x timed out", (ULONG_PTR)lpObj->lpRequestInfo);

			// remove
			InternalRequestTimeoutRemoveObject(lpHandler, lpObj);
		}

		lpObj = lpObj->next;
	}

	LeaveCriticalSection(&lpHandler->csLock);
}