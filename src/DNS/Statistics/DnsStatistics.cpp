#include "StdAfx.h"
#include "DnsStatistics.h"

DnsStatistics g_dnsStatistics;

static DWORD WINAPI DnsStatisticsThread(LPVOID lp)
{
    return static_cast<DnsStatistics*>(lp)->WorkerThread();
}

DnsStatistics::DnsStatistics() :
    m_hThread(NULL),
    m_hStopEvent(NULL),
    m_hChangedEvent(NULL)
{
    InitializeSRWLock(&m_lock);
    ZeroMemory(&m_stats, sizeof(DNS_STATISTICS));

    QueryPerformanceFrequency(&m_liFrequency);

    m_hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    m_hChangedEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    m_hThread = CreateThread(NULL, 0, DnsStatisticsThread, this, 0, NULL);

    //////////////////////////////////////////////////////////////////////////////

    GetSystemTime(&m_stats.Started);
    SetEvent(m_hChangedEvent);
}

DnsStatistics::~DnsStatistics()
{
    SetEvent(m_hStopEvent);
    if (WaitForSingleObject(m_hThread, 5000) == WAIT_TIMEOUT)
    {
        Error(__FUNCTION__ " - Thread could not be gracefully shut down (terminated).");
        TerminateThread(m_hThread, 0);
    }
    CloseHandle(m_hThread);
    CloseHandle(m_hChangedEvent);
    CloseHandle(m_hStopEvent);
}

void DnsStatistics::GetConnectionString(char* pbuf, size_t bufSize)
{
    sprintf_s(
        pbuf,
        bufSize,
        "Driver={SQL Server};SERVER=%s;Database=%s;Trusted_Connection=True",
        g_Configuration.SQL.Server,
        g_Configuration.SQL.DatabaseName);
}

void DnsStatistics::WriteChangesToSql()
{
    if (!g_Configuration.SQL.Enabled)
        return;

#ifndef __DOCKER
    SQLClient client;
    char connectionString[1024];
    GetConnectionString(connectionString, ARRAYSIZE(connectionString));
    if (!client.Open(connectionString))
    {
        const SQLErrorDescription& error = client.GetErrorDescription();
        Error(__FUNCTION__ " - SQL Connect failed: %s - %s", error.SqlState, error.Message);
        return;
    }

    DNS_STATISTICS stats;
    GetStatistics(&stats);

    char query[1024];
    sprintf_s(query, 1024,
        "EXEC [dbo].[UpdateServiceStatus] @status=1, @started='%04d-%02d-%02d %02d:%02d:%02d', @resolved=%I64d, @failed=%I64d, @cached=%I64d, @cacheHits=%I64d, @avgResponse=%I64d",
        stats.Started.wYear,
        stats.Started.wMonth,
        stats.Started.wDay,
        stats.Started.wHour,
        stats.Started.wMinute,
        stats.Started.wSecond,
        stats.Resolved,
        stats.Failed,
        stats.Cached,
        stats.CacheHits,
        stats.AverageRequestTimes);
    client.Execute(query);
#endif
}

DWORD DnsStatistics::WorkerThread()
{
    HANDLE hEvents[] = { m_hStopEvent, m_hChangedEvent };

    while (1)
    {
        DWORD dw = WaitForMultipleObjects(ARRAYSIZE(hEvents), hEvents, FALSE, INFINITE);
        if (dw == 0)
            break;

        if (dw == 1)
            WriteChangesToSql();
    }

    return 0;
}