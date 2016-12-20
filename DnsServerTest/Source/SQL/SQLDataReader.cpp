#include "StdAfx.h"
#include "SQLClient.h"

SQLDataReader::SQLDataReader(SQLClient* pSqlClient, PVOID hStmt) :
    m_pSqlClient(pSqlClient),
    m_hStmt(hStmt)
{
    ZeroMemory(&m_errorDescription, sizeof(SQLErrorDescription));
}

SQLDataReader::~SQLDataReader()
{
    if (m_hStmt)
    {
        SQLFreeHandle(SQL_HANDLE_STMT, m_hStmt);
        m_hStmt = NULL;
    }
}

SQLRETURN SQLDataReader::Fetch()
{
    SQLRETURN ret = SQLFetch(m_hStmt);
    if (ret != SQL_SUCCESS &&
        ret != SQL_NO_DATA)
        SQLClient::GetSqlErrorDescription(SQL_HANDLE_STMT, m_hStmt, &m_errorDescription);

    return ret;
}

BOOL SQLDataReader::GetBoolean(int column)
{
    char c;
    SQLGetData(m_hStmt, (SQLUSMALLINT)column, SQL_C_BIT, &c, sizeof(c), NULL);
    return c;
}

unsigned char SQLDataReader::GetInt8(int column)
{
    unsigned char c;
    SQLGetData(m_hStmt, (SQLUSMALLINT)column, SQL_C_TINYINT, &c, sizeof(c), NULL);
    return c;
}

short SQLDataReader::GetInt16(int column)
{
    short c;
    SQLGetData(m_hStmt, (SQLUSMALLINT)column, SQL_C_SSHORT, &c, sizeof(c), NULL);
    return c;
}

int SQLDataReader::GetInt32(int column)
{
    int c;
    SQLGetData(m_hStmt, (SQLUSMALLINT)column, SQL_C_SLONG, &c, sizeof(c), NULL);
    return c;
}

__int64 SQLDataReader::GetInt64(int column)
{
    __int64 c;
    SQLGetData(m_hStmt, (SQLUSMALLINT)column, SQL_C_SBIGINT, &c, sizeof(c), NULL);
    return c;
}

time_t SQLDataReader::GetDateTimeTicks(int column)
{
    TIMESTAMP_STRUCT ts = {};
    SQLGetData(m_hStmt, (SQLUSMALLINT)column, SQL_C_TIMESTAMP, &ts, sizeof(ts), NULL);

    struct tm tm = {};
    tm.tm_year = ts.year - 1900;
    tm.tm_mon = ts.month - 1;
    tm.tm_mday = ts.day;
    tm.tm_hour = ts.hour;
    tm.tm_min = ts.minute;
    tm.tm_sec = ts.second;
    return mktime(&tm);
}

void SQLDataReader::GetString(int column, char* buf, int bufSize)
{
    SQLGetData(m_hStmt, column, SQL_C_CHAR, (SQLPOINTER)buf, bufSize, NULL);
}