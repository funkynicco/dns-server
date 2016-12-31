#include "StdAfx.h"

#ifndef _SERVICE
BOOL g_bShutdown = FALSE;

void InitializeConsole();
int AppMain(LPWSTR* argv, int argc);

int wmain(char**, int) // not using these, see the GetCommandLineW below instead
{
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    return AppMain(argv, argc);
}
#else // _SERVICE
void WINAPI ServiceMain(DWORD argc, LPTSTR* argv); // Service.cpp

int wmain(int argc, TCHAR** argv)
{
    WriteToErrorFile(L"Starting ...");

    SERVICE_TABLE_ENTRY ServiceTable[] =
    {
        { SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain },
        { NULL, NULL }
    };

    if (!StartServiceCtrlDispatcher(ServiceTable))
    {
        WCHAR buf[1024];
        swprintf(buf, 1024, L"StartServiceCtrlDispatcher error code: %u", GetLastError());
        WriteToErrorFile(buf);
        return GetLastError();
    }

    WriteToErrorFile(L"CtrlDispatcher OK");
    return 0;
}
#endif // _SERVICE

int AppStartup(LPWSTR* argv, int argc, Configuration& config)
{
    WCHAR buf[1024];

    WriteToErrorFile(__FUNCTION__ L" - loading");

    WCHAR filename[MAX_PATH];
    GetModuleFileName(NULL, filename, ARRAYSIZE(filename));
    WriteToErrorFile(filename);

    WCHAR directory[MAX_PATH];
    wcscpy_s(directory, filename);
    PathRemoveFileSpec(directory);

    for (int i = 1; i < argc; ++i)
    {
        LPWSTR arg = argv[i];
        if (_wcsicmp(arg, L"--directory") == 0)
        {
            if (i + 1 >= argc)
            {
                OutputDebugString(L"Error: --directory not found");
                return 1;
            }

            wcscpy_s(directory, argv[++i]);
        }
        else if (_wcsicmp(arg, L"--test-pipe") == 0)
        {
            HANDLE hPipe = CreateFile(L"\\\\.\\pipe\\dnsbootstrapper", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hPipe == INVALID_HANDLE_VALUE)
            {
                WCHAR msg[1024];
                swprintf(msg, 1024, L"Pipe test failed with code: %u", GetLastError());
                MessageBox(NULL, msg, L"Test pipe", MB_OK | MB_ICONWARNING);
            }
            else
            {
                CloseHandle(hPipe);
                MessageBox(NULL, L"Pipe test succeeded!", L"Test pipe", MB_OK | MB_ICONINFORMATION);
            }

            return 0;
        }
    }

    if (lstrlenW(directory) > 0 &&
        !SetCurrentDirectory(directory))
    {
        swprintf(buf, 1024, L"SetCurrentDirectory failed: %u", GetLastError());
        WriteToErrorFile(buf);
        return 1;
}

#ifndef _SERVICE
    InitializeConsole();
#endif // _CONSOLE

    if (!g_logger.Initialize())
    {
        swprintf(buf, 1024, L"Logger initialize failed: %u", GetLastError());
        WriteToErrorFile(buf);
        return 1;
    }

    WCHAR username[128];
    DWORD dw = ARRAYSIZE(username);
    GetUserNameW(username, &dw);
    Error(L"[%s] Starting bootstrap service ...", username);

    if (!config.Load(L"config.json"))
    {
        swprintf(buf, 1024, L"Load configuration file failed: %u", GetLastError());
        WriteToErrorFile(buf);
        return 1;
    }

    return 0;
}

#ifndef _SERVICE
int AppMain(LPWSTR* argv, int argc)
{
    Configuration config;

    int ret = AppStartup(argv, argc, config);
    if (ret != 0)
        return ret;

    Bootstrapper bootstrapper(config);

    while (!g_bShutdown)
        Sleep(100);

    return 0;
}

static BOOL WINAPI HandlerRoutine(DWORD dwCtrlType)
{
    switch (dwCtrlType)
    {
    case CTRL_C_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        g_bShutdown = TRUE;
        return TRUE;
    }

    return FALSE;
}

static void InitializeConsole()
{
    SetConsoleCtrlHandler(HandlerRoutine, TRUE);
    SetConsoleTitle(L"DNS Bootstrapper Service");
}
#endif // _SERVICE