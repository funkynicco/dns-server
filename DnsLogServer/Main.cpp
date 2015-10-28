#include "StdAfx.h"

int main(int argc, char* argv[])
{
	SetConsoleTitle(L"DNS Log Server");

	WSADATA wd;
	WSAStartup(MAKEWORD(2, 2), &wd);

	SOCKADDR_IN addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(16650);

	LogServer logServer;

	if (logServer.Start(&addr))
	{
		printf(
			"Started log server on %d.%d.%d.%d:%d\n",
			(addr.sin_addr.s_addr) & 0xff,
			(addr.sin_addr.s_addr >> 8) & 0xff,
			(addr.sin_addr.s_addr >> 16) & 0xff,
			(addr.sin_addr.s_addr >> 24) & 0xff,
			ntohs(addr.sin_port));

		while (1)
		{
			if (_kbhit() &&
				_getch() == VK_ESCAPE)
				break;

			Sleep(100);
		}

		logServer.Stop();
	}

	WSACleanup();
	return 0;
}