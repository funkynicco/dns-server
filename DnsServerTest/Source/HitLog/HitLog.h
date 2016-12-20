#pragma once

struct HitLogItem;
class HitLog
{
public:
    HitLog();
    ~HitLog();

    void AddHit(const char* domain);

private:
    friend DWORD WINAPI HitLogThread(LPVOID lp);
    DWORD WorkerThread();
    void ProcessQueue();

    CRITICAL_SECTION m_cs;

    //SQLClient m_sql;
    
    HANDLE m_hThread;
    HANDLE m_hItemEvent;
    HANDLE m_hStopEvent;

    HitLogItem* m_pFreeHitLogItems;
    HitLogItem* m_pItemQueueHead;
    HitLogItem* m_pItemQueueTail;
};

extern HitLog g_hitLog;