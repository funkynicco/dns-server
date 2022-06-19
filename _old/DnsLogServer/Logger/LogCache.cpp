#include "StdAfx.h"
#include "LogCache.h"

LogCache::LogCache() :
	m_pFreeLogItem(NULL)
{
	InitializeCriticalSectionAndSpinCount(&m_cs, 2000);
}

LogCache::~LogCache()
{
	for (vector<void*>::const_iterator it = m_vMemoryBlocks.cbegin(); it != m_vMemoryBlocks.cend(); ++it)
		free(*it);

	DeleteCriticalSection(&m_cs);
}

LPLOG_ITEM LogCache::AllocateLogItem()
{
	LPLOG_ITEM lpItem = m_pFreeLogItem;
	if (!lpItem)
	{
		// allocate a block of memory
		void* lpMem;
		ASSERT(lpMem = malloc(sizeof(LOG_ITEM) * 32));
		m_vMemoryBlocks.push_back(lpMem);

		lpItem = (LPLOG_ITEM)lpMem; // first

		LPLOG_ITEM lpItemIter = (LPLOG_ITEM)lpMem + 1;
		for (int i = 0; i < 31; ++i)
		{
			lpItemIter->next = m_pFreeLogItem;
			m_pFreeLogItem = lpItemIter++;
		}
	}
	else
		m_pFreeLogItem = m_pFreeLogItem->next;

	ZeroMemory(lpItem, sizeof(LOG_ITEM));
	return lpItem;
}

void LogCache::DestroyLogItem(LPLOG_ITEM lpItem)
{
	lpItem->next = m_pFreeLogItem;
	m_pFreeLogItem = lpItem;
}

void LogCache::AddLogItem(DWORD dwThread, int line, const char* filename, int flags, LONGLONG id, time_t tmTime, const char* message)
{
	EnterCriticalSection(&m_cs);
	LPLOG_ITEM lpItem = AllocateLogItem();

	lpItem->Line = line;
	strcpy(lpItem->Filename, filename);

	lpItem->LogMessage.dwThread = dwThread;
	lpItem->LogMessage.Flags = flags;
	lpItem->LogMessage.Id = id;
	lpItem->LogMessage.tmTime = tmTime;
	strcpy(lpItem->LogMessage.Message, message);

	lpItem->LogMessage.dwLength = 0; // not used ...

	m_logs.insert(pair<LONGLONG, LPLOG_ITEM>(id, lpItem));

	LeaveCriticalSection(&m_cs);
}