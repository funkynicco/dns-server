#include "StdAfx.h"
#include "DnsHosts.h"

#include <sqlext.h>

DnsHosts g_dnsHosts;

class SRWLocker
{
public:
    SRWLocker(PSRWLOCK pLock, BOOL bExclusive) :
        m_pLock(pLock),
        m_bExclusive(bExclusive)
    {
        if (m_bExclusive)
            AcquireSRWLockExclusive(m_pLock);
        else
            AcquireSRWLockShared(m_pLock);
    }

    virtual ~SRWLocker()
    {
        if (m_bExclusive)
            ReleaseSRWLockExclusive(m_pLock);
        else
            ReleaseSRWLockShared(m_pLock);
    }

private:
    PSRWLOCK m_pLock;
    BOOL m_bExclusive;
};

////////////////////////////////////////////

DnsHosts::DnsHosts() :
    m_tmDateChanged(0)
{
    InitializeSRWLock(&m_lock);
}

DnsHosts::~DnsHosts()
{
    for (auto it : m_hosts)
    {
        free(it.second);
    }
}

///////////////////////////////////////////////////////

inline BOOL IsWildcardDomain(const char* domain)
{
    for (; *domain; ++domain)
    {
        if (*domain == '*' ||
            *domain == '?')
            return TRUE;
    }

    return FALSE;
}

BOOL DnsHosts::Load()
{
    Error(__FUNCTION__ " - Loading domains from database ...");

    if (!g_Configuration.SQL.Enabled)
        return TRUE;

#ifndef __DOCKER
    SQLClient client;

    char connectionString[1024];
    GetConnectionString(connectionString, ARRAYSIZE(connectionString));
    if (!client.Open(connectionString))
    {
        const SQLErrorDescription& error = client.GetErrorDescription();
        Error(__FUNCTION__ " - SQL Connect failed: %s - %s", error.SqlState, error.Message);
        return FALSE;
    }
    ///////////////// connected ////////

    SQLDataReader reader = client.Execute("EXEC [dbo].[ServiceGetDomains]");
    if (!reader.Succeeded())
    {
        const SQLErrorDescription& error = reader.GetErrorDescription();
        Error(__FUNCTION__ " - SQL Query failed: %s - %s", error.SqlState, error.Message);
        return FALSE;
    }

    SRWLocker exclusiveLock(&m_lock, TRUE); // released when going out of function scope

    /////////// clear current //////////

    for (auto it : m_hosts)
    {
        free(it.second);
    }

    m_hosts.clear();

    m_wildcardHosts.clear();

    ////////////////////////////////////

    while (1)
    {
        SQLRETURN ret = reader.Fetch();
        if (ret == SQL_NO_DATA)
            break;

        if (ret != SQL_SUCCESS)
        {
            const SQLErrorDescription& error = client.GetErrorDescription();
            Error(__FUNCTION__ " - SQL Query fetch failed: %s - %s", error.SqlState, error.Message);
            return FALSE;
        }

        LPDNS_HOST_INFO lpHost = (LPDNS_HOST_INFO)malloc(sizeof(DNS_HOST_INFO)); // TODO: memory pooling
        ZeroMemory(lpHost, sizeof(DNS_HOST_INFO));
        char szIP[16];
        reader.GetString(1, lpHost->Domain, ARRAYSIZE(lpHost->Domain));
        reader.GetString(2, szIP, ARRAYSIZE(szIP));

        BOOL bIsWildcard = IsWildcardDomain(lpHost->Domain);

        lpHost->dwIP = inet_addr(szIP);

        if (bIsWildcard)
        {
            m_wildcardHosts.push_back(*lpHost);
            free(lpHost);
        }
        else
            m_hosts.insert(pair<string, LPDNS_HOST_INFO>(lpHost->Domain, lpHost));
    }

    ////////////////////////////////////

    SQLDataReader reader2 = client.Execute("SELECT [DateChanged] FROM [dbo].[ChangeTracking] WHERE [Table] = 'Hosts'");
    if (!reader2.Succeeded())
    {
        const SQLErrorDescription& error = reader2.GetErrorDescription();
        Error(__FUNCTION__ " - SQL Query 2 failed: %s - %s", error.SqlState, error.Message);
        return FALSE;
    }

    if (reader2.Fetch() == SQL_SUCCESS)
    {
        m_tmDateChanged = reader2.GetDateTimeTicks(1);
        //Error(__FUNCTION__ " - Load m_tmDateChanged: %I64d", m_tmDateChanged);
    }

    Error(__FUNCTION__ " - Load completed.");
#endif

    return TRUE;
}

void DnsHosts::PollReload()
{
    if (!g_Configuration.SQL.Enabled)
        return;

    //Error(__FUNCTION__ " - Running reload check on DB ...");

    BOOL bDoLoad = FALSE;
    {
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

        SQLDataReader reader = client.Execute("SELECT [DateChanged] FROM [dbo].[ChangeTracking] WHERE [Table] = 'Hosts'");
        if (!reader.Succeeded())
        {
            const SQLErrorDescription& error = reader.GetErrorDescription();
            Error(__FUNCTION__ " - SQL Query 2 failed: %s - %s", error.SqlState, error.Message);
            return;
        }

        if (reader.Fetch() == SQL_SUCCESS)
        {
            time_t tm = reader.GetDateTimeTicks(1);
            if (tm != m_tmDateChanged)
            {
                //Error(__FUNCTION__ " - tm: %I64d not same as nDateChanged: %I64d", tm, m_tmDateChanged);
                bDoLoad = TRUE;
            }
        }
#endif
    }

    //Error(__FUNCTION__ " - Running check => %s", bDoLoad ? "True" : "False");

    if (bDoLoad)
        Load();
}

void DnsHosts::GetConnectionString(char* pbuf, size_t bufSize)
{
    sprintf_s(
        pbuf,
        bufSize,
        "Driver={SQL Server};SERVER=%s;Database=%s;Trusted_Connection=True",
        g_Configuration.SQL.Server,
        g_Configuration.SQL.DatabaseName);
}