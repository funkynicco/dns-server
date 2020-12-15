#include "StdAfx.h"

void WriteToErrorFile(LPCWSTR pszText)
{
    HANDLE hFile = CreateFile(L"C:\\errors\\error.txt", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        SetFilePointer(hFile, 0, NULL, FILE_END);
        DWORD dw;

        if (GetFileSize(hFile, NULL) > 0)
            WriteFile(hFile, "\r\n", 2, &dw, NULL);

        char buffer[16384];
        buffer[WideCharToMultiByte(CP_UTF8, 0, pszText, -1, buffer, sizeof(buffer), NULL, NULL)] = 0;

        WriteFile(hFile, buffer, (DWORD)strlen(buffer), &dw, NULL);

        CloseHandle(hFile);
    }
}