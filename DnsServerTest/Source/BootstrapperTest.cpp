#include "StdAfx.h"

int RunBootstrapTest()
{
    HANDLE hPipe = CreateFile(
        L"\\\\.\\pipe\\dnsbootstrapper",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
        NULL);
    if (hPipe == INVALID_HANDLE_VALUE)
    {
        wprintf(__FUNCTION__ L" - Connect to pipe failed with code: %u\n", GetLastError());
        return 1;
    }

    WCHAR username[128];
    DWORD dw = ARRAYSIZE(username);
    GetUserName(username, &dw);

    HANDLE hIOEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    OVERLAPPED Overlapped;

    wprintf(L"Hello world from user [%s]\n", username);
    while (1)
    {
        ResetEvent(hIOEvent);
        Overlapped = {};
        Overlapped.hEvent = hIOEvent;
        char buf[8192];
        DWORD dw;
        if (!ReadFile(hPipe, buf, 8192, &dw, &Overlapped) &&
            GetLastError() != ERROR_IO_PENDING)
        {
            wprintf(__FUNCTION__ L" - ReadFile exit code: %u\n", GetLastError());
            break;
        }

        if (WaitForSingleObject(hIOEvent, 10000) == WAIT_TIMEOUT)
        {
            wprintf(__FUNCTION__ L" - No ping received after 10 seconds (closing ...)\n");
            break;
        }

        //wprintf(__FUNCTION__ L" - Received %u bytes\n", dw);
        int command = *(int*)buf;
        if (command == -1)
        {
            wprintf(__FUNCTION__ L" - Closing ...\n");
            break;
        }
        else if (command == 1)
        {
            ResetEvent(hIOEvent);
            Overlapped = {};
            Overlapped.hEvent = hIOEvent;
            int ping_response = 10;
            if (!WriteFile(hPipe, &ping_response, 4, &dw, &Overlapped) &&
                GetLastError() != ERROR_IO_PENDING)
            {
                wprintf(__FUNCTION__ L" - WriteFile exit code: %u\n", GetLastError());
                break;
            }

            if (WaitForSingleObject(hIOEvent, 5000) == WAIT_TIMEOUT)
            {
                wprintf(__FUNCTION__ L" - Failed to send ping response after 5 seconds.\n");
                break;
            }
        }
    }

    CloseHandle(hIOEvent);
    CloseHandle(hPipe);
    return 0;
}