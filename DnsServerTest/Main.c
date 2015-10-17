#include "StdAfx.h"

#ifdef __LOG_SERVER_IO
void CheckOutstandingIO(LPSERVER_INFO lpServerInfo)
{
	EnterCriticalSection(&lpServerInfo->csStats);
	DWORD dwPendingWSARecvFrom = lpServerInfo->lpPendingWSARecvFrom->dwElementNum;
	DWORD dwPendingWSASendTo = lpServerInfo->lpPendingWSASendTo->dwElementNum;
	LeaveCriticalSection(&lpServerInfo->csStats);

	static BOOL bInitial = TRUE;
	static DWORD dwOldPendingWSARecvFrom = 0;
	static DWORD dwOldPendingWSASendTo = 0;

	if (bInitial ||
		dwPendingWSARecvFrom != dwOldPendingWSARecvFrom ||
		dwPendingWSASendTo != dwOldPendingWSASendTo)
	{
		LoggerWrite(__FUNCTION__ " - RECV: %u, SEND: %u", dwPendingWSARecvFrom, dwPendingWSASendTo);

		bInitial = FALSE;
		dwOldPendingWSARecvFrom = dwPendingWSARecvFrom;
		dwOldPendingWSASendTo = dwPendingWSASendTo;
	}
}
#endif // __LOG_SERVER_IO

void PrintMap(LPPTR_MAP lpMap)
{
	PTR_MAP_ITERATOR Iter;
	PtrMapResetIterator(lpMap, &Iter);

	while (PtrMapIteratorFetch(&Iter))
	{
		DWORD dwKey = Iter.lpCurrentEntry->dwKey;
		LPVOID lpValue = Iter.lpCurrentEntry->Ptr;

		printf("%u = %08x\n", dwKey, (DWORD_PTR)lpValue);

		PtrMapIteratorDelete(&Iter);
	}
}

void TestPtrMap()
{
	char timeResult[256];
	srand(5999);

	LARGE_INTEGER liFrequency, liStart, liEnd;
	QueryPerformanceFrequency(&liFrequency);

	LPPTR_MAP lpMap = CreatePtrMap(65535);

	while (1)
	{
		QueryPerformanceCounter(&liStart);
		PtrMapClear(lpMap);
		QueryPerformanceCounter(&liEnd);
		GetFrequencyCounterResult(timeResult, liFrequency, liStart, liEnd);
		printf("PtrMapClear => %s\n", timeResult);

		DWORD dwFind;
		const int MaxInserts = 5000;
		DWORD dwIndexToFind = rand() % MaxInserts;

		QueryPerformanceCounter(&liStart);
		for (int i = 0; i < 5000; ++i)
		{
			DWORD dwKey = rand();

			if (i == dwIndexToFind)
				dwFind = dwKey;

			PtrMapInsert(lpMap, dwKey, i);
		}
		QueryPerformanceCounter(&liEnd);
		GetFrequencyCounterResult(timeResult, liFrequency, liStart, liEnd);
		printf("PtrMapInsert %d elements => %s\n", MaxInserts, timeResult);

		QueryPerformanceCounter(&liStart);
		void* ret = PtrMapFind(lpMap, dwFind);
		QueryPerformanceCounter(&liEnd);
		GetFrequencyCounterResult(timeResult, liFrequency, liStart, liEnd);
		printf("PtrMapFind [%u => %u] = %u - %s\n", dwIndexToFind, dwFind, (DWORD_PTR)ret, timeResult);


		printf("\n\n");
		while (1)
		{
			if (_kbhit() && _getch())
				break;

			Sleep(50);
		}
	}

	DestroyPtrMap(lpMap);
}

int main(int argc, char* argv[])
{
	LoggerInitialize(); // This must always be first because all logging depend on it

	SetConsoleTitle(L"DNS Server Test");

	TestPtrMap();
	return 0;

	WSADATA wd;
	WSAStartup(MAKEWORD(2, 2), &wd);

	InitializeSocketPool();

	LPSERVER_INFO lpServerInfo = AllocateServerInfo();
	if (lpServerInfo)
	{
		SOCKADDR_IN SecondaryDnsServer;
		MakeSocketAddress(&SecondaryDnsServer, "8.8.8.8", 53);
		ServerSetSecondaryDnsServer(lpServerInfo, &SecondaryDnsServer);

		const int Port = 53; // default 53 - 5666

		if (StartServer(lpServerInfo, Port))
		{
			Error("/// Started on port %d ///", Port);
			while (1)
			{
				if (_kbhit() && _getch() == VK_ESCAPE)
					break;

#ifdef __LOG_SERVER_IO
				CheckOutstandingIO(lpServerInfo);
#endif // __LOG_SERVER_IO

				RequestTimeoutProcess(lpServerInfo->lpRequestTimeoutHandler);

				// (Maintenance) Fill up the socket pool
				SocketPoolFill();

				Sleep(100);
		}

			Error("/// STOPPING SERVER ///");
			StopServer(lpServerInfo);
		}
		else
			Error("Failed to start server, socket error: %d - %s", GetLastError(), GetErrorMessage(GetLastError()));

#ifdef __LOG_ALLOCATIONS
		Error("/// Deleting server instance ///");
#endif // __LOG_ALLOCATIONS
		DestroyServerInfo(lpServerInfo);
	}
	else
		Error("Failed to allocate server info");

	DestroySocketPool();
	LoggerDestroy();

	WSACleanup();
	//system("pause");
	return 0;
}