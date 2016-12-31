#include "StdAfx.h"
#include "Bootstrapper.h"

#include <AccCtrl.h>
#include <AclAPI.h>

static DWORD WINAPI BootstrapperThread(LPVOID lp)
{
    return static_cast<Bootstrapper*>(lp)->WorkerThread();
}

Bootstrapper::Bootstrapper(Configuration& config) :
    m_config(config),
    m_hThread(NULL),
    m_hStopEvent(NULL),
    m_hReadIOEvent(NULL),
    m_hWriteIOEvent(NULL),
    m_bShowPingTimes(FALSE)
{
    m_hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    m_hReadIOEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    m_hWriteIOEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    m_hThread = CreateThread(NULL, 0, BootstrapperThread, this, 0, NULL);

    m_config.GetValueAsBool(L"DnsServer:ShowPingTimes", &m_bShowPingTimes);
}

Bootstrapper::~Bootstrapper()
{
    SetEvent(m_hStopEvent);
    for (int i = 0; i < 5; ++i) // waits for 5*5=25 seconds until terminating thread
    {
        if (WaitForSingleObject(m_hThread, 5000) == WAIT_OBJECT_0)
            break;

        Error(L"(Shutdown) WorkerThread not responding ...");

        if (i + 1 >= 5)
        {
            TerminateThread(m_hThread, 0);
            Error(L"Forcefully terminated thread");
        }
    }

    CloseHandle(m_hThread);
    CloseHandle(m_hStopEvent);
    CloseHandle(m_hReadIOEvent);
    CloseHandle(m_hWriteIOEvent);
}

inline void GracefullyShutdownOrTerminateProcess(HANDLE hPipe, HANDLE hProcess)
{
    Error(L"Stopping process ...");

    // send shutdown signal with pipe
    int shutdownCommand = -1;
    DWORD dw;
    if (!WriteFile(hPipe, &shutdownCommand, 4, &dw, NULL))
        Error(L"WriteFile failed with code: %u", GetLastError());

    if (WaitForSingleObject(hProcess, 5000) == WAIT_TIMEOUT)
    {
        Error(L"Process didnt exit after 5 seconds (terminated)");
        TerminateProcess(hProcess, 0);
    }
    else
        Error(L"Process stopped");
}

inline BOOL SendPing(HANDLE hPipe, HANDLE hReadEvent, HANDLE hWriteEvent, DWORD dwTimeout, BOOL bShowPingTimes)
{
    OVERLAPPED OverlappedRead = {};
    OVERLAPPED OverlappedWrite = {};
    OverlappedRead.hEvent = hReadEvent;
    OverlappedWrite.hEvent = hWriteEvent;

    ResetEvent(hReadEvent);
    ResetEvent(hWriteEvent);

    DWORD dw;
    int ping = 1;
    int response;

    // post a read
    if (!ReadFile(hPipe, &response, sizeof(response), &dw, &OverlappedRead) &&
        GetLastError() != ERROR_IO_PENDING)
    {
        Error(L"ReadFile failed with code: %u", GetLastError());
        return FALSE;
    }

    // send ping and expect a response
    LARGE_INTEGER liFrequency, liStart, liEnd;
    if (bShowPingTimes)
    {
        QueryPerformanceFrequency(&liFrequency);
        QueryPerformanceCounter(&liStart);
    }
    // post write
    if (!WriteFile(hPipe, &ping, sizeof(ping), &dw, &OverlappedWrite) &&
        GetLastError() != ERROR_IO_PENDING)
    {
        Error(L"WriteFile failed with code: %u", GetLastError());
        return FALSE;
    }

    if (WaitForSingleObject(hReadEvent, dwTimeout) == WAIT_TIMEOUT)
    {
        Error(L"Ping timeout");
        return FALSE;
    }

    if (bShowPingTimes)
    {
        QueryPerformanceCounter(&liEnd);

        double tm = double(liEnd.QuadPart - liStart.QuadPart) / liFrequency.QuadPart;
        if (tm < 1.0)
        {
            tm *= 1000.0;
            if (tm < 1.0)
            {
                tm *= 1000.0;
                if (tm < 1.0)
                {
                    tm *= 1000.0;
                    Error(L"Ping time: %f ns", tm);
                }
                else
                    Error(L"Ping time: %f us", tm);
            }
            else
                Error(L"Ping time: %f ms", tm);
        }
        else
            Error(L"Ping time: %f s", tm);
    }

    if (response != 10)
    {
        Error(L"DNS Server ping response was not 10: %d", response);
        return FALSE;
    }

    return TRUE;
}

DWORD Bootstrapper::WorkerThread()
{
    int firstStartDelay = 1;
    m_config.GetValueAsInt(L"DnsServer:FirstStartDelay", &firstStartDelay);

    BOOL restartOnFailure = TRUE;
    m_config.GetValueAsBool(L"DnsServer:RestartOnFailure", &restartOnFailure);

    int restartDelay = 1;
    m_config.GetValueAsInt(L"DnsServer:RestartDelay", &restartDelay);

    PROCESS_INFORMATION processInformation = {};

    int delayWait = firstStartDelay;

    while (1)
    {
        if (WaitForSingleObject(m_hStopEvent, delayWait * 1000) == WAIT_OBJECT_0)
            break;

        delayWait = restartDelay;

        HANDLE hPipe = CreatePipe();
        if (hPipe == INVALID_HANDLE_VALUE)
        {
            Error(L"Failed to create pipe, code: %u", GetLastError());
            return 1;
        }

        processInformation = {};
        if (!StartProcess(&processInformation))
        {
            Error(L"Failed to start process, code: %u", GetLastError());
            CloseHandle(hPipe);
            return 1;
        }

        Error(L"Started process, id: %u", processInformation.dwProcessId);

        // wait for process and monitor it ...
        OVERLAPPED Overlapped = {};
        Overlapped.hEvent = m_hReadIOEvent;
        ResetEvent(m_hReadIOEvent);

        // post connect check
        if (!ConnectNamedPipe(hPipe, &Overlapped) &&
            GetLastError() != ERROR_PIPE_CONNECTED &&
            GetLastError() != ERROR_IO_PENDING)
        {
            Error(L"ConnectNamedPipe failed with code: %u", GetLastError());
            TerminateProcess(processInformation.hProcess, 0);
            CloseHandle(processInformation.hThread);
            CloseHandle(processInformation.hProcess);
            CloseHandle(hPipe);
            return 1;
        }

        ResumeThread(processInformation.hThread);

        if (WaitForSingleObject(m_hReadIOEvent, 50000) == WAIT_TIMEOUT)
        {
            Error(L"ConnectNamedPipe timed out after 5 seconds");
            TerminateProcess(processInformation.hProcess, 0);
            CloseHandle(processInformation.hThread);
            CloseHandle(processInformation.hProcess);
            CloseHandle(hPipe);
            return 1;
        }

        Error(L"DNS Server connected to pipe.");

        HANDLE hEvents[] = { m_hStopEvent, processInformation.hProcess };
        while (1)
        {
            if (!SendPing(hPipe, m_hReadIOEvent, m_hWriteIOEvent, 2000, m_bShowPingTimes))
            {
                GracefullyShutdownOrTerminateProcess(hPipe, processInformation.hProcess);
                break;
            }

            DWORD dw = WaitForMultipleObjects(ARRAYSIZE(hEvents), hEvents, FALSE, 5000);
            if (dw == 0) // stop
            {
                // gracefully shut down process
                GracefullyShutdownOrTerminateProcess(hPipe, processInformation.hProcess);
                break;
            }

            if (dw == 1) // process exited unexpectedly
            {
                DWORD dwExitCode = 0;
                GetExitCodeProcess(processInformation.hProcess, &dwExitCode);

                Error(L"Process exited unexpectedly. Exit code: %u", dwExitCode);
                break;
            }
        }

        CloseHandle(processInformation.hThread);
        CloseHandle(processInformation.hProcess);
        CloseHandle(hPipe);
    }

    return 0;
}

BOOL Bootstrapper::StartProcess(LPPROCESS_INFORMATION lpProcessInformation)
{
    WCHAR executable[MAX_PATH] = {};
    m_config.GetValue(L"DnsServer:Executable", executable, ARRAYSIZE(executable));

    WCHAR workingDirectory[MAX_PATH] = {};
    m_config.GetValue(L"DnsServer:WorkingDirectory", workingDirectory, ARRAYSIZE(workingDirectory));

    WCHAR arguments[MAX_PATH] = {};
    m_config.GetValue(L"DnsServer:Arguments", arguments, ARRAYSIZE(arguments));

    BOOL createNoWindow = TRUE;
    m_config.GetValueAsBool(L"DnsServer:CreateNoWindow", &createNoWindow);

    BOOL loadUserProfile = FALSE;
    m_config.GetValueAsBool(L"DnsServer:LoadUserProfile", &loadUserProfile);

    struct
    {
        BOOL Enabled = FALSE;
        WCHAR Username[128] = {};
        WCHAR Password[128] = {};
    } StartAs;

    m_config.GetValueAsBool(L"DnsServer:StartAs:Enabled", &StartAs.Enabled);
    m_config.GetValue(L"DnsServer:StartAs:Username", StartAs.Username, ARRAYSIZE(StartAs.Username));
    m_config.GetValue(L"DnsServer:StartAs:Password", StartAs.Password, ARRAYSIZE(StartAs.Password));

    /////////////////////////////////////////////////////////////////////////////////////////////////////////

    DWORD dwFlags = createNoWindow ? CREATE_NO_WINDOW : CREATE_NEW_CONSOLE;

    dwFlags |= CREATE_SUSPENDED;

    STARTUPINFO startupInfo = {};
    startupInfo.cb = sizeof(startupInfo);

    if (StartAs.Enabled)
    {
        DWORD dwLogonFlags = loadUserProfile ? LOGON_WITH_PROFILE : 0;

        return CreateProcessWithLogonW(
            StartAs.Username,
            NULL,
            StartAs.Password,
            dwLogonFlags,
            executable,
            arguments,
            dwFlags,
            NULL,
            workingDirectory,
            &startupInfo,
            lpProcessInformation);
    }

    return CreateProcess(
        executable,
        arguments,
        NULL,
        NULL,
        TRUE,
        dwFlags,
        NULL,
        workingDirectory,
        &startupInfo,
        lpProcessInformation);
}

HANDLE Bootstrapper::CreatePipe()
{
    struct
    {
        BOOL Enabled = FALSE;
        WCHAR Username[128] = {};
    } StartAs;

    m_config.GetValueAsBool(L"DnsServer:StartAs:Enabled", &StartAs.Enabled);
    m_config.GetValue(L"DnsServer:StartAs:Username", StartAs.Username, ARRAYSIZE(StartAs.Username));

    SecurityAttributes sa;
    LPSECURITY_ATTRIBUTES lpSecurityAttributes = NULL;

    if (StartAs.Enabled)
    {
        lpSecurityAttributes = sa
            .AddCurrentUser(GENERIC_ALL)
            .GrantUser(StartAs.Username, GENERIC_READ | GENERIC_WRITE)
            .Build();

        if (lpSecurityAttributes == NULL)
            return NULL;
    }

    return CreateNamedPipe(
        L"\\\\.\\pipe\\dnsbootstrapper",
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE,
        1,
        8192,
        8192,
        0,
        lpSecurityAttributes);
}