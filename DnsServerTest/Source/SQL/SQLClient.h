#pragma once

#include <sqlext.h>

struct SQLErrorDescription
{
    SQLCHAR SqlState[6];
    SQLCHAR Message[2048];
    SQLINTEGER NativeError;
};

class SQLClient;
class SQLDataReader
{
public:
    SQLDataReader(SQLClient* pSqlClient, PVOID hStmt);
    ~SQLDataReader();

    // move operator
    inline SQLDataReader(SQLDataReader&& reader)
    {
        m_pSqlClient = reader.m_pSqlClient;
        m_hStmt = reader.m_hStmt;
        memcpy(&m_errorDescription, &reader.m_errorDescription, sizeof(SQLErrorDescription));

        reader.m_pSqlClient = NULL;
        reader.m_hStmt = NULL;
    }

    inline BOOL Succeeded() { return m_hStmt != NULL; }

    SQLRETURN Fetch();

    BOOL GetBoolean(int column);
    unsigned char GetInt8(int column);
    short GetInt16(int column);
    int GetInt32(int column);
    __int64 GetInt64(int column);
    __int64 GetDateTimeTicks(int column);
    void GetString(int column, char* buf, int bufSize);

private:
    friend class SQLClient;
    SQLClient* m_pSqlClient;
    PVOID m_hStmt;
    SQLErrorDescription m_errorDescription;
};

class SQLClient
{
public:
    SQLClient();
    virtual ~SQLClient();
    BOOL Open(const char* connectionString);
    void Close();

    SQLDataReader Execute(const char* query);

private:
    friend class SQLDataReader;
    static BOOL GetSqlErrorDescription(SQLSMALLINT handleType, PVOID pHandle, SQLErrorDescription* pError);

    PVOID m_hEnv;
    PVOID m_hCon;
    SQLErrorDescription m_errorDescription;
};