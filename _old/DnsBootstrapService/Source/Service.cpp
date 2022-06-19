#include "StdAfx.h"

#ifdef _SERVICE

void WINAPI ServiceCtrlHandler(DWORD dwCtrlCode);
DWORD WINAPI ServiceWorkerThread(LPVOID lpParam);

int AppStartup(LPWSTR* argv, int argc, Configuration& config); // Main.cpp

/////////////////////////////////////////////////

static SERVICE_STATUS_HANDLE g_hServiceStatus = NULL;
static HANDLE g_hServiceStopEvent = NULL;

inline void ReportStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint)
{
    static DWORD dwCheckPoint = 1;

    SERVICE_STATUS status = {};
    status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status.dwCurrentState = dwCurrentState;
    status.dwWin32ExitCode = dwWin32ExitCode;
    status.dwWaitHint = dwWaitHint;

    if (dwCurrentState == SERVICE_START_PENDING)
        status.dwControlsAccepted = 0;
    else
        status.dwControlsAccepted = SERVICE_ACCEPT_STOP;

    if (dwCurrentState == SERVICE_RUNNING ||
        dwCurrentState == SERVICE_STOPPED)
        status.dwCheckPoint = 0;
    else
        status.dwCheckPoint = dwCheckPoint++;

    if (!SetServiceStatus(g_hServiceStatus, &status))
    {
        WCHAR buf[1024];
        swprintf(buf, 1024, L"SetServiceStatus failed with code: %u", GetLastError());
        WriteToErrorFile(buf);
    }
}

void WINAPI ServiceMain(DWORD argc, LPTSTR* argv)
{
    WriteToErrorFile(__FUNCTION__ L" called");

    g_hServiceStatus = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceCtrlHandler);
    if (!g_hServiceStatus)
    {
        WCHAR buf[1024];
        swprintf(buf, 1024, L"RegisterServiceCtrlHandler failed with code: %u", GetLastError());
        WriteToErrorFile(buf);
        return;
    }

    WriteToErrorFile(L"RegisterServiceCtrlHandler OK");

    ReportStatus(SERVICE_START_PENDING, 0, 3000);

    g_hServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!g_hServiceStopEvent)
    {
        WriteToErrorFile(__FUNCTION__ L" - Failed to create event");
        ReportStatus(SERVICE_STOPPED, GetLastError(), 0);
        return;
    }

    Configuration config;
    int ret = AppStartup(argv, (int)argc, config);

    WCHAR buf[1024];
    swprintf(buf, 1024, L"AppStartup ret: %d", ret);
    WriteToErrorFile(buf);

    if (ret == 0)
    {
        // startup ok
        Bootstrapper bootstrapper(config);

        ReportStatus(SERVICE_RUNNING, 0, 0);

        HANDLE hEvents[] = { g_hServiceStopEvent, bootstrapper.GetThread() };
        DWORD dw = WaitForMultipleObjects(ARRAYSIZE(hEvents), hEvents, FALSE, INFINITE);

        swprintf(buf, 1024, L"WaitForMultipleObjects result: %u", dw);
        WriteToErrorFile(buf);

        if (dw == 1)
        {
            // the bootstrapper thread unexpectedly exited
            if (!GetExitCodeThread(hEvents[1], &dw))
                dw = 1;
            ret = (int)dw;
        }

        ReportStatus(SERVICE_STOP_PENDING, 0, 0);
    } // <--- bootstrapper will try to close the sub process in the destructor which is called at end of scope here

    ReportStatus(SERVICE_STOPPED, (DWORD)ret, 0);
    CloseHandle(g_hServiceStopEvent);
    g_hServiceStopEvent = NULL;
}

void WINAPI ServiceCtrlHandler(DWORD dwCtrlCode)
{
    switch (dwCtrlCode)
    {
    case SERVICE_CONTROL_STOP:
        SetEvent(g_hServiceStopEvent);
        break;
    }
}

#endif // _CONSOLE