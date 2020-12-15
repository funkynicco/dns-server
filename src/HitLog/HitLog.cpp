#include "StdAfx.h"
#include "HitLog.h"

HitLog g_hitLog;
DWORD WINAPI HitLogThread(LPVOID lp);

struct HitLogItem
{
    char Domain[64];

    struct HitLogItem* prev;
    struct HitLogItem* next;
};

///////////////////////////////////////////////////////////////

HitLog::HitLog() :
    m_hThread(NULL),
    m_pFreeHitLogItems(NULL),
    m_pItemQueueHead(NULL),
    m_pItemQueueTail(NULL)
{
    InitializeCriticalSectionAndSpinCount(&m_cs, 2000);

    m_hItemEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    m_hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    m_hThread = CreateThread(NULL, 0, HitLogThread, this, 0, NULL);
}

HitLog::~HitLog()
{
    SetEvent(m_hStopEvent);
    if (WaitForSingleObject(m_hThread, 5000) == WAIT_TIMEOUT)
    {
        TerminateThread(m_hThread, 0);
        Error(__FUNCTION__ " - Thread did not gracefully stop after 5 seconds (terminated)");
    }
    CloseHandle(m_hThread);

    while (m_pFreeHitLogItems)
    {
        HitLogItem* next = m_pFreeHitLogItems->next;
        free(m_pFreeHitLogItems);
        m_pFreeHitLogItems = next;
    }
    while (m_pItemQueueHead)
    {
        HitLogItem* next = m_pItemQueueHead->next;
        free(m_pItemQueueHead);
        m_pItemQueueHead = next;
    }

    DeleteCriticalSection(&m_cs);

    CloseHandle(m_hItemEvent);
    CloseHandle(m_hStopEvent);
}

void HitLog::AddHit(const char* domain)
{
    if (!g_Configuration.SQL.Enabled)
        return;

    EnterCriticalSection(&m_cs);

    HitLogItem* item = m_pFreeHitLogItems;
    if (item)
        m_pFreeHitLogItems = m_pFreeHitLogItems->next;
    else
        item = (HitLogItem*)malloc(sizeof(HitLogItem));

    strcpy(item->Domain, domain);
    DualLinkedListAddTail(m_pItemQueueHead, m_pItemQueueTail, item);

    LeaveCriticalSection(&m_cs);
    SetEvent(m_hItemEvent);
}

void HitLog::ProcessQueue()
{
    SQLClient client;
    char connectionString[1024];
    sprintf_s(
        connectionString,
        ARRAYSIZE(connectionString),
        "Driver={SQL Server};SERVER=%s;Database=%s;Trusted_Connection=True",
        g_Configuration.SQL.Server,
        g_Configuration.SQL.DatabaseName);
    if (!client.Open(connectionString))
    {
        const SQLErrorDescription& error = client.GetErrorDescription();
        Error(__FUNCTION__ " - SQL Connect failed: %s - %s", error.SqlState, error.Message);
        return;
    }

    // unlink the current queue
    EnterCriticalSection(&m_cs);
    HitLogItem* head = m_pItemQueueHead;
    m_pItemQueueHead = m_pItemQueueTail = NULL;
    LeaveCriticalSection(&m_cs);

    for (HitLogItem* item = head; item; item = item->next)
    {
        // process item
        char query[256];
        sprintf(query, "EXEC [dbo].[AddHit] @domain='%s'", item->Domain);
        SQLDataReader reader = client.Execute(query);
        if (!reader.Succeeded())
        {
            const SQLErrorDescription& error = reader.GetErrorDescription();
            Error(__FUNCTION__ " - SQL AddHit Query failed: %s - %s", error.SqlState, error.Message);
        }
    }

    // add back the items to the free slots
    EnterCriticalSection(&m_cs);
    while (head)
    {
        HitLogItem* item = head;
        head = head->next;

        item->next = m_pFreeHitLogItems;
        m_pFreeHitLogItems = item;
    }
    LeaveCriticalSection(&m_cs);
}

DWORD HitLog::WorkerThread()
{
    HANDLE hEvents[2] = { m_hItemEvent, m_hStopEvent };

    while (1)
    {
        DWORD dw = WaitForMultipleObjects(ARRAYSIZE(hEvents), hEvents, FALSE, INFINITE);
        if (dw == 0)
            ProcessQueue();
        else if (dw == 1)
            break;
    }

    return 0;
}

DWORD WINAPI HitLogThread(LPVOID lp)
{
    return static_cast<HitLog*>(lp)->WorkerThread();
}