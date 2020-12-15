#include "StdAfx.h"
#include "Logger.h"

Logger g_logger;

Logger::Logger() :
    m_hFile(INVALID_HANDLE_VALUE)
{
    InitializeCriticalSectionAndSpinCount(&m_cs, 2000);
}

Logger::~Logger()
{
    DeleteCriticalSection(&m_cs);

    if (m_hFile != INVALID_HANDLE_VALUE)
        CloseHandle(m_hFile);
}

BOOL Logger::Initialize()
{
    if (m_hFile != INVALID_HANDLE_VALUE)
        return FALSE;

    CreateDirectory(L"logs", NULL);

    SYSTEMTIME st;
    GetLocalTime(&st);

    WCHAR filename[MAX_PATH];
    wsprintf(filename, L"logs\\log-%04d%02d%02d_%02d%02d%02d.txt", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    m_hFile = CreateFile(filename, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    return m_hFile != INVALID_HANDLE_VALUE;
}

inline BOOL WriteFileEx(HANDLE hFile, LPCWSTR text, int length = -1)
{
    if (length == -1)
        length = lstrlenW(text);

    length *= sizeof(WCHAR);

    DWORD dwPos = 0;
    DWORD dwWritten;

    while (dwPos < (DWORD)length)
    {
        if (!WriteFile(hFile, text + dwPos, length - dwPos, &dwWritten, NULL))
            break;

        dwPos += dwWritten;
    }

    return dwPos == length;
}

void Logger::InternalWrite(LPCWSTR text)
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t time[64];
    wsprintf(time, L"[%04d-%02d-%02d %02d:%02d:%02d] ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    EnterCriticalSection(&m_cs);
    WriteFileEx(m_hFile, time);
    WriteFileEx(m_hFile, text);
    WriteFileEx(m_hFile, L"\r\n", 2);
    FlushFileBuffers(m_hFile);

#ifndef _SERVICE
    wprintf(L"%s%s\n", time, text);
#endif // _CONSOLE

    LeaveCriticalSection(&m_cs);
}