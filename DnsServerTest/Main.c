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

int main(int argc, char* argv[])
{
	LoggerInitialize(); // This must always be first because all logging depend on it

	SetConsoleTitle(L"DNS Server Test");

	WSADATA wd;
	WSAStartup(MAKEWORD(2, 2), &wd);

	InitializeSocketPool();

	LPSERVER_INFO lpServerInfo = AllocateServerInfo();
	if (lpServerInfo)
	{
		const int Port = 5666; // default 53 - 5666

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