#include "StdAfx.h"
#include "Console.h"

static DWORD WINAPI ConsoleThread(LPVOID lp);

#define ITEMS_PER_BLOCKS		128
#define MAX_ADD_CONSOLE_ITEMS	2048

typedef struct _tagConsoleItem
{
	int LogType;
	char Message[1024];
	int MessageLength;
	SYSTEMTIME stTime;

	struct _tagConsoleItem* prev;
	struct _tagConsoleItem* next;

} CONSOLE_ITEM, *LPCONSOLE_ITEM;

static struct
{
	HANDLE hConsole;
	HANDLE hThread;
	HANDLE hStopEvent;
	CRITICAL_SECTION CriticalSection;

	LPCONSOLE_ITEM lpConsoleItemsHead;
	LPCONSOLE_ITEM lpConsoleItemsTail;

	LPCONSOLE_ITEM lpFreeConsoleItems;

	ARRAY_CONTAINER ConsoleItemBlocksArray;

	LPCONSOLE_ITEM AddConsoleItemsAry[MAX_ADD_CONSOLE_ITEMS];
	DWORD dwAddConsoleItemsCount;

} g_Console;

__inline LPCONSOLE_ITEM AllocateConsoleItem()
{
	LPCONSOLE_ITEM lpItem = g_Console.lpFreeConsoleItems;
	if (!lpItem)
	{
		void* block = malloc(sizeof(CONSOLE_ITEM) * ITEMS_PER_BLOCKS);
		if (!block)
		{
			MessageBox(0, L"Ran out of memory in AllocateConsoleItem", L"Memory", MB_ICONERROR);
			return NULL;
		}

		ArrayContainerAddElement(&g_Console.ConsoleItemBlocksArray, block, NULL);

		for (int i = 0; i < ITEMS_PER_BLOCKS; ++i)
		{
			LPCONSOLE_ITEM lp = (LPCONSOLE_ITEM)block + i;

			if (i > 0)
			{
				lp->next = g_Console.lpFreeConsoleItems;
				g_Console.lpFreeConsoleItems = lp;
			}
			else
				lpItem = lp;
		}
	}
	else
		g_Console.lpFreeConsoleItems = g_Console.lpFreeConsoleItems->next;

	return lpItem;
}

__inline void FreeConsoleItem(LPCONSOLE_ITEM lpItem)
{
	lpItem->next = g_Console.lpFreeConsoleItems;
	g_Console.lpFreeConsoleItems = lpItem;
}

__inline const char* GetLogTypeString(int logType)
{
	switch (logType)
	{
	case LT_NORMAL: return "Normal";
	case LT_DEBUG: return "Debug";
	case LT_WARNING: return "Warning";
	case LT_ERROR: return "Error";
	}

	return "Unknown";
}

__inline void ProcessConsoleItem(LPCONSOLE_ITEM lpItem)
{
	switch (lpItem->LogType)
	{
	case LT_DEBUG:
		SetConsoleTextAttribute(g_Console.hConsole, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
		break;
	case LT_WARNING:
		SetConsoleTextAttribute(g_Console.hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
		break;
	case LT_ERROR:
		SetConsoleTextAttribute(g_Console.hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
		break;
	}

	printf(
		"[%02d:%02d:%02d][%s] %s\n",
		lpItem->stTime.wHour,
		lpItem->stTime.wMinute,
		lpItem->stTime.wSecond,
		GetLogTypeString(lpItem->LogType),
		lpItem->Message);

	SetConsoleTextAttribute(g_Console.hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

__inline void ConsoleProcess()
{
	EnterCriticalSection(&g_Console.CriticalSection);

	if (g_Console.dwAddConsoleItemsCount == 0)
	{
		LeaveCriticalSection(&g_Console.CriticalSection);
		return;
	}

	// reset cmd

	for (DWORD i = 0; i < g_Console.dwAddConsoleItemsCount; ++i)
	{
		LPCONSOLE_ITEM lpItem = g_Console.AddConsoleItemsAry[i];

		ProcessConsoleItem(lpItem);
	}
	g_Console.dwAddConsoleItemsCount = 0;

	// restore cmd

	LeaveCriticalSection(&g_Console.CriticalSection);
}

void ConsoleInitialize()
{
    ZeroMemory(&g_Console, sizeof(g_Console));

    g_Console.hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    g_Console.hThread = CreateThread(NULL, 0, ConsoleThread, NULL, CREATE_SUSPENDED, NULL);
    g_Console.hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    InitializeCriticalSection(&g_Console.CriticalSection);
    ArrayContainerCreate(&g_Console.ConsoleItemBlocksArray, 128);

    ResumeThread(g_Console.hThread);
}

void ConsoleDestroy()
{
    SetEvent(g_Console.hStopEvent);
    while (WaitForSingleObject(g_Console.hThread, 2000) == WAIT_TIMEOUT)
        printf(__FUNCTION__ " - THREAD STOP TIMEOUT\n");

    CloseHandle(g_Console.hThread);
    CloseHandle(g_Console.hStopEvent);

    ConsoleProcess(); // write all currently pending items

    for (DWORD i = 0; i < g_Console.ConsoleItemBlocksArray.dwElementNum; ++i)
    {
        free(g_Console.ConsoleItemBlocksArray.pElem[i]);
    }

    ArrayContainerDestroy(&g_Console.ConsoleItemBlocksArray);
    DeleteCriticalSection(&g_Console.CriticalSection);
}

void ConsoleWrite(int logType, const char* format, ...)
{
    va_list l;
    va_start(l, format);

    EnterCriticalSection(&g_Console.CriticalSection);
    LPCONSOLE_ITEM lpItem = AllocateConsoleItem();

    int len = vsprintf_s(lpItem->Message, sizeof(lpItem->Message), format, l);
    ASSERT(len > 0);

    va_end(l);

    lpItem->MessageLength = len;
    lpItem->LogType = logType;
    GetLocalTime(&lpItem->stTime);

    DualLinkedListAddTail(g_Console.lpConsoleItemsHead, g_Console.lpConsoleItemsTail, lpItem);

    ASSERT(g_Console.dwAddConsoleItemsCount < MAX_ADD_CONSOLE_ITEMS);
    g_Console.AddConsoleItemsAry[g_Console.dwAddConsoleItemsCount++] = lpItem;
    LeaveCriticalSection(&g_Console.CriticalSection);
}

static DWORD WINAPI ConsoleThread(LPVOID lp)
{
	while (1)
	{
		if (WaitForSingleObject(g_Console.hStopEvent, 50) == WAIT_OBJECT_0)
			break;

		ConsoleProcess();
	}

	return 0;
}