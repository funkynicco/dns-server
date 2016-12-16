#include "StdAfx.h"
#include "SQLClient.h"

SQLClient::SQLClient() :
    m_hEnv(NULL),
    m_hCon(NULL)
{
    ZeroMemory(&m_errorDescription, sizeof(SQLErrorDescription));
}

SQLClient::~SQLClient()
{
    Close();
}

BOOL SQLClient::Open(const char* connectionString)
{
    if (SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_hEnv) != SQL_SUCCESS)
        return FALSE;

    if (SQLSetEnvAttr(m_hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0) != SQL_SUCCESS)
    {
        Close();
        return FALSE;
    }

    if (SQLAllocHandle(SQL_HANDLE_DBC, m_hEnv, &m_hCon) != SQL_SUCCESS)
    {
        Close();
        return FALSE;
    }

    SQLCHAR szRetConnectionString[1024];

    switch (SQLDriverConnectA(m_hCon, NULL, (SQLCHAR*)connectionString, SQL_NTS, szRetConnectionString, 1024, NULL, SQL_DRIVER_NOPROMPT))
    {
    case SQL_INVALID_HANDLE:
    case SQL_ERROR:
        GetSqlErrorDescription(SQL_HANDLE_DBC, m_hCon, &m_errorDescription);
        Close();
        return FALSE;
    }

    return TRUE;
}

void SQLClient::Close()
{
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

SQLDataReader SQLClient::Execute(const char* query)
{
    PVOID hStmt;
    if (SQLAllocHandle(SQL_HANDLE_STMT, m_hCon, &hStmt) != SQL_SUCCESS)
    {
        SQLDataReader reader(this, NULL);
        GetSqlErrorDescription(SQL_HANDLE_DBC, m_hCon, &reader.m_errorDescription);
        return reader;
    }

    if (SQLExecDirectA(hStmt, (SQLCHAR*)query, SQL_NTS) != SQL_SUCCESS)
    {
        SQLDataReader reader(this, NULL);
        GetSqlErrorDescription(SQL_HANDLE_STMT, hStmt, &reader.m_errorDescription);
        return reader;
    }

    return SQLDataReader(this, hStmt);
}

BOOL SQLClient::GetSqlErrorDescription(SQLSMALLINT handleType, PVOID pHandle, SQLErrorDescription* pError)
{
    SQLSMALLINT nLen;
    if (SQLGetDiagRecA(handleType, pHandle, 1, pError->SqlState, &pError->NativeError, (SQLCHAR*)pError->Message, ARRAYSIZE(pError->Message), &nLen) != SQL_SUCCESS)
        return FALSE;

    pError->Message[nLen] = 0;
    return TRUE;
}