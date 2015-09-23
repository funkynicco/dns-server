#include "StdAfx.h"

int main(int argc, char* argv[])
{
	SetConsoleTitle(L"DNS Server Test");

	WSADATA wd;
	WSAStartup(MAKEWORD(2, 2), &wd);

	LPSERVER_INFO lpServerInfo = AllocateServerInfo();
	if (lpServerInfo)
	{
		if (StartServer(lpServerInfo, 53))
		{
			printf("Started on port %d\n", 53);
			while (1)
			{
				if (_kbhit() && _getch() == VK_ESCAPE)
					return;

				ProcessServer(lpServerInfo);
				Sleep(50);
			}

			StopServer(lpServerInfo);
		}
		else
			printf("Failed to start server, socket error: %d\n", GetLastError());

		DestroyServerInfo(lpServerInfo);
	}
	else
		printf("Failed to allocate server info\n");

	WSACleanup();
	system("pause");
	return 0;
}