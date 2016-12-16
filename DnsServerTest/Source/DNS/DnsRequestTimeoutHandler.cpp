#include "StdAfx.h"
#include "DnsServer.h"

#define MAX_MEMORY_BLOCKS	256
#define OBJECTS_PER_BLOCK	256

struct LinkedListObject
{
	LPDNS_REQUEST_INFO lpRequestInfo;
	time_t tmTimeout;

	struct LinkedListObject* prev;
	struct LinkedListObject* next;
};

typedef struct _tagDnsRequestTimeoutHandler
{
	void* MemBlocks[MAX_MEMORY_BLOCKS];
	DWORD dwMemBlocks;
	CRITICAL_SECTION csLock;

	struct LinkedListObject* lpFreeRequests;
	struct LinkedListObject* lpAllocatedRequests;

} DNS_REQUEST_TIMEOUT_HANDLER, *LPDNS_REQUEST_TIMEOUT_HANDLER;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

LPDNS_REQUEST_TIMEOUT_HANDLER DnsRequestTimeoutCreate()
{
	LPDNS_REQUEST_TIMEOUT_HANDLER lpHandler = (LPDNS_REQUEST_TIMEOUT_HANDLER)malloc(sizeof(DNS_REQUEST_TIMEOUT_HANDLER));
	ZeroMemory(lpHandler, sizeof(DNS_REQUEST_TIMEOUT_HANDLER));

	InitializeCriticalSection(&lpHandler->csLock);

	return lpHandler;
}

void DnsRequestTimeoutDestroy(LPDNS_REQUEST_TIMEOUT_HANDLER lpHandler)
{
	for (DWORD i = 0; i < lpHandler->dwMemBlocks; ++i)
		VirtualFree(lpHandler->MemBlocks[i], 0, MEM_RELEASE);

	DeleteCriticalSection(&lpHandler->csLock);
}

static BOOL InternalDnsRequestTimeoutCreateBlock(LPDNS_REQUEST_TIMEOUT_HANDLER lpHandler)
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

void DnsRequestTimeoutSetCancelTimeout(LPDNS_REQUEST_TIMEOUT_HANDLER lpHandler, LPDNS_REQUEST_INFO lpRequestInfo, time_t tmTimeout)
{
	EnterCriticalSection(&lpHandler->csLock);

	struct LinkedListObject* lpObj;
	for (lpObj = lpHandler->lpAllocatedRequests; lpObj; lpObj = lpObj->next)
	{
		if (lpObj->lpRequestInfo == lpRequestInfo)
		{
			Error(__FUNCTION__ " - lpRequestInfo %p already found in lpAllocatedRequests", lpRequestInfo);
			break;
		}
	}

	if (!lpObj)
	{
		if (!lpHandler->lpFreeRequests &&
			!InternalDnsRequestTimeoutCreateBlock(lpHandler))
		{
			LeaveCriticalSection(&lpHandler->csLock);
			Error(__FUNCTION__ " - Failed to allocate more blocks for lpRequestInfo: %p", lpRequestInfo);
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

static void InternalDnsRequestTimeoutRemoveObject(LPDNS_REQUEST_TIMEOUT_HANDLER lpHandler, struct LinkedListObject* lpObj)
{
	if (lpObj->next)
		lpObj->next->prev = lpObj->prev;
	if (lpObj->prev)
		lpObj->prev->next = lpObj->next;
	if (lpObj == lpHandler->lpAllocatedRequests)
		lpHandler->lpAllocatedRequests = lpHandler->lpAllocatedRequests->next;
}

void DnsRequestTimeoutRemoveRequest(LPDNS_REQUEST_TIMEOUT_HANDLER lpHandler, LPDNS_REQUEST_INFO lpRequestInfo)
{
	EnterCriticalSection(&lpHandler->csLock);

	struct LinkedListObject* lpObj;
	for (lpObj = lpHandler->lpAllocatedRequests; lpObj; lpObj = lpObj->next)
	{
		if (lpObj->lpRequestInfo == lpRequestInfo)
		{
			InternalDnsRequestTimeoutRemoveObject(lpHandler, lpObj);
			break;
		}
	}

	LeaveCriticalSection(&lpHandler->csLock);
}

void DnsRequestTimeoutProcess(LPDNS_REQUEST_TIMEOUT_HANDLER lpHandler)
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
				Error(__FUNCTION__ " - CancelIoEx [lpRequestInfo: %p, Socket: %Id] failed with code %u - %s",
					lpObj->lpRequestInfo,
					lpObj->lpRequestInfo->Socket,
					GetLastError(),
					GetErrorMessage(GetLastError()));
			}
			else
				Error(__FUNCTION__ " - %p timed out", lpObj->lpRequestInfo);

			// remove
			InternalDnsRequestTimeoutRemoveObject(lpHandler, lpObj);
		}

		lpObj = lpObj->next;
	}

	LeaveCriticalSection(&lpHandler->csLock);
}