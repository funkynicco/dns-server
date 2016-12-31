#include "StdAfx.h"

#ifdef _CONSOLE
void InitializeConsole();
#endif // _CONSOLE

BOOL g_bShutdown = FALSE;

#ifdef _CONSOLE
int wmain(char**, int) // not using these, see the GetCommandLineW below instead
#else // _CONSOLE
int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
#endif // _CONSOLE
{
#if 0
    SecurityAttributes sa;
    LPSECURITY_ATTRIBUTES lpSecurityAttributes = sa
        .AddCurrentUser(GENERIC_ALL)
        //.GrantUser(L"Nicco", GENERIC_ALL)
        .GrantUser(L"dns.service", GENERIC_READ)
        .Build();

    // play with creating a file, then we can see the permissions of it...
    HANDLE hTestFile = CreateFile(L"testfile2.txt", GENERIC_READ, FILE_SHARE_READ, lpSecurityAttributes, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hTestFile == INVALID_HANDLE_VALUE)
    {
        WCHAR msg[1024];
        swprintf(msg, 1024, L"createfile code: %u", GetLastError());
        MessageBox(NULL, msg, L"create file", MB_OK | MB_ICONERROR);
    }
    else
        CloseHandle(hTestFile);
    return 0;
#endif // 0

    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    WCHAR directory[MAX_PATH];
    wcscpy_s(directory, argv[0]);
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
        return 1;

#ifdef _CONSOLE
    InitializeConsole();
#endif // _CONSOLE

    if (!g_logger.Initialize())
        return 1;

    WCHAR username[128];
    DWORD dw = ARRAYSIZE(username);
    GetUserNameW(username, &dw);
    Error(L"[%s] Starting bootstrap service ...", username);

    Configuration config;
    if (!config.Load(L"config.json"))
        return 1;

    Bootstrapper bootstrapper(config);

    while (!g_bShutdown)
        Sleep(100);

    return 0;
}

#ifdef _CONSOLE
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
#endif // _CONSOLE