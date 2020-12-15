#pragma once

typedef struct _tagLogItem
{
	int Line;
	char Filename[MAX_PATH];

	LOG_MESSAGE LogMessage; // NetworkDefines.h

	struct _tagLogItem* next;

} LOG_ITEM, *LPLOG_ITEM;

class LogCache
{
public:
	LogCache();
	virtual ~LogCache();

	void AddLogItem(DWORD dwThread, int line, const char* filename, int flags, LONGLONG id, time_t tmTime, const char* message);

	static LogCache* GetInstance()
	{
		static LogCache instance;
		return &instance;
	}

	inline map<LONGLONG, LPLOG_ITEM>& GetLogs() { return m_logs; }
	inline void Lock() { EnterCriticalSection(&m_cs); }
	inline void Unlock() { LeaveCriticalSection(&m_cs); }

private:
	CRITICAL_SECTION m_cs;
	LPLOG_ITEM m_pFreeLogItem;
	vector<void*> m_vMemoryBlocks;

	map<LONGLONG, LPLOG_ITEM> m_logs;

	LPLOG_ITEM AllocateLogItem();
	void DestroyLogItem(LPLOG_ITEM lpItem);
};