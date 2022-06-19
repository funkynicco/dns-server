#include "StdAfx.h"
#include "Bootstrapper.h"

static DWORD WINAPI BootstrapperThread(LPVOID lp)
{
    static_cast<Bootstrapper*>(lp)->WorkerThread();
    return 0;
}

Bootstrapper::Bootstrapper() :
    m_hThread(NULL),
    m_hStopEvent(NULL),
    m_hPipe(INVALID_HANDLE_VALUE)
{
    m_hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
}

Bootstrapper::~Bootstrapper()
{
    if (m_hThread)
    {
        SetEvent(m_hStopEvent);
        if (WaitForSingleObject(m_hThread, 2000) == WAIT_TIMEOUT)
        {
            Error("Bootstrapper thread failed to close");
            TerminateThread(m_hThread, 0);
        }

        CloseHandle(m_hThread);
    }

    if (m_hPipe != INVALID_HANDLE_VALUE)
        CloseHandle(m_hPipe);

    CloseHandle(m_hStopEvent);
}

BOOL Bootstrapper::Connect()
{
    if (m_hPipe != INVALID_HANDLE_VALUE)
        return TRUE;

    Error(__FUNCTION__ " - Connecting to pipe ...");

    m_hPipe = CreateFile(
        L"\\\\.\\pipe\\dnsbootstrapper",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
        NULL);
    if (m_hPipe == INVALID_HANDLE_VALUE)
    {
        Error(__FUNCTION__ " - Connect to pipe failed with code: %u\n", GetLastError());
        return FALSE;
    }

    Error(__FUNCTION__ " - Connected to pipe");

    if (m_hThread)
    {
        if (WaitForSingleObject(m_hThread, 2000) == WAIT_TIMEOUT)
            TerminateThread(m_hThread, 0);

        CloseHandle(m_hThread);
    }

    m_hThread = CreateThread(NULL, 0, BootstrapperThread, this, 0, NULL);
    return TRUE;
}

BOOL Bootstrapper::WaitForExit(DWORD dwMilliseconds)
{
    return WaitForSingleObject(m_hThread, dwMilliseconds) == WAIT_OBJECT_0;
}

void Bootstrapper::WorkerThread()
{
    OVERLAPPED Overlapped;
    HANDLE hIOEvent = CreateEvent(NULL, FALSE, FALSE, NULL); // testing: setting manual reset to FALSE

    char buf[8192];
    DWORD dw;

    while (1)
    {
        Overlapped = {};
        Overlapped.hEvent = hIOEvent;
        //ResetEvent(hIOEvent);

        if (!ReadFile(m_hPipe, buf, 8192, &dw, &Overlapped) &&
            GetLastError() != ERROR_IO_PENDING)
        {
            Error(__FUNCTION__ " - ReadFile returned: %u", GetLastError());
            break;
        }

        if (WaitForSingleObject(hIOEvent, 10000) == WAIT_TIMEOUT)
        {
            Error(__FUNCTION__ " - No ping received after 10 seconds (closing ...)\n");
            break;
        }

        int command = *(int*)buf;
        if (command == -1)
        {
            Error(__FUNCTION__ " - Closing ...\n");
            break;
        }
        else if (command == 1)
        {
            //ResetEvent(hIOEvent);
            Overlapped = {};
            Overlapped.hEvent = hIOEvent;
            int ping_response = 10;
            if (!WriteFile(m_hPipe, &ping_response, 4, &dw, &Overlapped) &&
                GetLastError() != ERROR_IO_PENDING)
            {
                Error(__FUNCTION__ " - WriteFile returned: %u\n", GetLastError());
                break;
            }

            if (WaitForSingleObject(hIOEvent, 5000) == WAIT_TIMEOUT)
            {
                Error(__FUNCTION__ " - Failed to send ping response after 5 seconds.\n");
                break;
            }
        }
    }

    CloseHandle(hIOEvent);

    int q = 0;
}