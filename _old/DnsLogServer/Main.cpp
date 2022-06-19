#include "StdAfx.h"

void TestHttpParse()
{
	const char* str = "GET / HTTP/1.1\na: 0\n\n";
	size_t str_len = strlen(str);

	CIRCULAR_BUFFER buffer;
	CircularBufferCreate(&buffer, 1024);
	buffer.dwStart = 1024 - 10;
	CircularBufferAppend(&buffer, str, (DWORD)str_len);

	DWORD dwOffset;
	HTTP_PARAM param;
	auto result = ParseHttpHeader(&param, &buffer, &dwOffset);
	printf(__FUNCTION__ " - ParseHttpHeader returned %s\n", GetParseHeaderResultText(result));
	if (result == PHR_SUCCEEDED)
	{
		// read the data ...

		CircularBufferRemove(&buffer, dwOffset); // if any data, then we need to remove it too ...
	}

	CircularBufferDestroy(&buffer);
}

int main(int argc, char* argv[])
{
	InitializeErrorDescriptionTable();
	SetConsoleTitle(L"DNS Log Server");

	/*TestHttpParse();
	return 0;*/

	//LogCache::GetInstance()->AddLogItem(1, __LINE__, __FILE__, 0, 1, time(NULL), "Hello World");

	if (!g_cfg.Load("config.cfg"))
	{
		printf(__FUNCTION__ " - Failed to load configuration.\n");
		return 1;
	}

	WSADATA wd;
	WSAStartup(MAKEWORD(2, 2), &wd);

	SOCKADDR_IN webAddr;
	webAddr.sin_family = AF_INET;
	webAddr.sin_addr.s_addr = inet_addr(g_cfg.Web.NetworkInterface);
	webAddr.sin_port = htons(g_cfg.Web.Port);

	SOCKADDR_IN logAddr;
	logAddr.sin_family = AF_INET;
	logAddr.sin_addr.s_addr = inet_addr(g_cfg.Log.NetworkInterface);
	logAddr.sin_port = htons(g_cfg.Log.Port);

	LogServer logServer;
	LogWebServer webServer;

	if (webServer.Start(&webAddr)) // TODO: change so web/log servers can be enabled/disabled in config.cfg
	{
		printf(
			"Started web server on %d.%d.%d.%d:%d\n",
			(webAddr.sin_addr.s_addr) & 0xff,
			(webAddr.sin_addr.s_addr >> 8) & 0xff,
			(webAddr.sin_addr.s_addr >> 16) & 0xff,
			(webAddr.sin_addr.s_addr >> 24) & 0xff,
			ntohs(webAddr.sin_port));

		if (logServer.Start(&logAddr))
		{
			printf(
				"Started log server on %d.%d.%d.%d:%d\n",
				(logAddr.sin_addr.s_addr) & 0xff,
				(logAddr.sin_addr.s_addr >> 8) & 0xff,
				(logAddr.sin_addr.s_addr >> 16) & 0xff,
				(logAddr.sin_addr.s_addr >> 24) & 0xff,
				ntohs(logAddr.sin_port));

			while (1)
			{
				if (_kbhit() &&
					_getch() == VK_ESCAPE)
					break;

				webServer.Process();

				Sleep(25);
			}

			logServer.Stop();
		}
		else
			printf("Could not start log server on port %d\n", ntohs(logAddr.sin_port));

		webServer.Stop();
	}
	else
		printf("Could not start web server on port %d\n", ntohs(webAddr.sin_port));

	WSACleanup();
	return 0;
}