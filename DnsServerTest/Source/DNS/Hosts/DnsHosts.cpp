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

DnsHosts::DnsHosts()
{
    InitializeSRWLock(&m_lock);
}

DnsHosts::~DnsHosts()
{
    for (auto it : m_hosts)
    {
        free(it.second);
    }

    DestroyHandles();
}

///////////////////////////////////////////////////////

BOOL DnsHosts::GetSqlStatementError()
{
    SQLSMALLINT nLen;
    if (SQLGetDiagRecA(SQL_HANDLE_STMT, m_hStmt, 1, m_szSqlState, &m_nNativeError, (SQLCHAR*)m_szMessage, ARRAYSIZE(m_szMessage), &nLen) != SQL_SUCCESS)
        return FALSE;

    m_szMessage[nLen] = 0;
    return TRUE;
}

BOOL DnsHosts::GetSqlConnectionError()
{
    SQLSMALLINT nLen;
    if (SQLGetDiagRecA(SQL_HANDLE_DBC, m_hCon, 1, m_szSqlState, &m_nNativeError, (SQLCHAR*)m_szMessage, ARRAYSIZE(m_szMessage), &nLen) != SQL_SUCCESS)
        return FALSE;

    m_szMessage[nLen] = 0;
    return TRUE;
}

void DnsHosts::DestroyHandles()
{
    if (m_hStmt)
    {
        SQLFreeHandle(SQL_HANDLE_STMT, m_hStmt);
        m_hStmt = NULL;
    }

    if (m_hCon)
    {
        SQLDisconnect(m_hCon);
        SQLFreeHandle(SQL_HANDLE_DBC, m_hCon);
        m_hCon = NULL;
    }

    if (m_hEnv)
    {
        SQLFreeHandle(SQL_HANDLE_ENV, m_hEnv);
        m_hEnv = NULL;
    }
}

BOOL DnsHosts::Load()
{
    if (!g_Configuration.SQL.Enabled)
        return TRUE;

    if (SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_hEnv) != SQL_SUCCESS)
        return FALSE;

    if (SQLSetEnvAttr(m_hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0) != SQL_SUCCESS)
    {
        DestroyHandles();
        return FALSE;
    }

    if (SQLAllocHandle(SQL_HANDLE_DBC, m_hEnv, &m_hCon) != SQL_SUCCESS)
    {
        DestroyHandles();
        return FALSE;
    }

    char connectionString[1024];
    sprintf_s(
        connectionString,
        ARRAYSIZE(connectionString),
        "Driver={SQL Server};SERVER=%s;Database=%s;Trusted_Connection=True",
        g_Configuration.SQL.Server,
        g_Configuration.SQL.DatabaseName);
    SQLCHAR szRetConnectionString[1024];

    switch (SQLDriverConnectA(m_hCon, NULL, (SQLCHAR*)connectionString, SQL_NTS, szRetConnectionString, 1024, NULL, SQL_DRIVER_NOPROMPT))
    {
    case SQL_INVALID_HANDLE:
    case SQL_ERROR:
        GetSqlConnectionError();
        Error(__FUNCTION__ " - SQL Connect failed: %s - %s", m_szSqlState, m_szMessage);
        DestroyHandles();
        return FALSE;
    }

    ///////////////// connected ////////

    if (SQLAllocHandle(SQL_HANDLE_STMT, m_hCon, &m_hStmt) != SQL_SUCCESS)
    {
        DestroyHandles();
        return FALSE;
    }

    if (SQLExecDirectA(m_hStmt, (SQLCHAR*)"SELECT [Domain], [IP] FROM [dbo].[Hosts]", SQL_NTS) != SQL_SUCCESS)
    {
        GetSqlStatementError();

        Error(__FUNCTION__ " - Query failed: %s - %s", m_szSqlState, m_szMessage);

        DestroyHandles();
        return FALSE;
    }

    SRWLocker exclusiveLock(&m_lock, TRUE); // released when going out of function scope

    /////////// clear current //////////

    for (auto it : m_hosts)
    {
        free(it.second);
    }

    m_hosts.clear();

    ////////////////////////////////////

    while (1)
    {
        SQLRETURN ret = SQLFetch(m_hStmt);
        if (ret == SQL_NO_DATA)
            break;

        if (ret != SQL_SUCCESS)
        {
            GetSqlStatementError();

            Error(__FUNCTION__ " - Query fetch failed: %s - %s", m_szSqlState, m_szMessage);

            DestroyHandles();
            return FALSE;
        }

        LPDNS_HOST_INFO lpHost = (LPDNS_HOST_INFO)malloc(sizeof(DNS_HOST_INFO)); // TODO: memory pooling
        ZeroMemory(lpHost, sizeof(DNS_HOST_INFO));
        char szIP[16];
        if ((ret = SQLGetData(m_hStmt, 1, SQL_C_CHAR, lpHost->Domain, ARRAYSIZE(lpHost->Domain), NULL)) != SQL_SUCCESS ||
            (ret = SQLGetData(m_hStmt, 2, SQL_C_CHAR, szIP, ARRAYSIZE(szIP), NULL)) != SQL_SUCCESS)
        {
            GetSqlStatementError();

            Error(__FUNCTION__ " - [ret: %d] Query fetch failed: %s - %s", ret, m_szSqlState, m_szMessage);

            DestroyHandles();
            return FALSE;
        }

        lpHost->dwIP = inet_addr(szIP);
        m_hosts.insert(pair<string, LPDNS_HOST_INFO>(lpHost->Domain, lpHost));
    }

    ////////////////////////////////////

    DestroyHandles();
    return TRUE;
}