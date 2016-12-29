#pragma once

typedef struct _tagDNS_STATISTICS
{
    SYSTEMTIME Started;
    LONGLONG Resolved;
    LONGLONG Failed;
    LONGLONG Cached;
    LONGLONG CacheHits;

} DNS_STATISTICS, *LPDNS_STATISTICS;

class DnsStatistics
{
public:
    DnsStatistics();
    ~DnsStatistics();

    void Startup() { } // causes constructor to be called

    inline LPDNS_STATISTICS BeginUpdate()
    {
        AcquireSRWLockExclusive(&m_lock);
        return &m_stats;
    }

    inline void EndUpdate()
    {
        SetEvent(m_hChangedEvent);
        ReleaseSRWLockExclusive(&m_lock);
    }

    inline void GetStatistics(LPDNS_STATISTICS lpStatistics)
    {
        AcquireSRWLockShared(&m_lock);
        CopyMemory(lpStatistics, &m_stats, sizeof(DNS_STATISTICS));
        ReleaseSRWLockShared(&m_lock);
    }

    // Warning: Do not use BeginUpdate() or EndUpdate() with this, it automatically locks
    inline void IncrementResolved()
    {
        BeginUpdate();
        ++m_stats.Resolved;
        EndUpdate();
    }

    // Warning: Do not use BeginUpdate() or EndUpdate() with this, it automatically locks
    inline void IncrementFailed()
    {
        BeginUpdate();
        ++m_stats.Failed;
        EndUpdate();
    }

    // Warning: Do not use BeginUpdate() or EndUpdate() with this, it automatically locks
    inline void SetCached(LONGLONG cached)
    {
        BeginUpdate();
        m_stats.Cached = cached;
        EndUpdate();
    }

    // Warning: Do not use BeginUpdate() or EndUpdate() with this, it automatically locks
    inline void IncrementCacheHits()
    {
        BeginUpdate();
        ++m_stats.CacheHits;
        EndUpdate();
    }

private:
    friend DWORD WINAPI DnsStatisticsThread(LPVOID);
    DWORD WorkerThread();
    void GetConnectionString(char* pbuf, size_t bufSize);
    void WriteChangesToSql();
    SRWLOCK m_lock;
    HANDLE m_hThread;
    HANDLE m_hStopEvent;
    HANDLE m_hChangedEvent;

    DNS_STATISTICS m_stats;
};

extern DnsStatistics g_dnsStatistics;