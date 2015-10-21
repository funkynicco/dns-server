#include "StdAfx.h"

#ifdef __LOG_DNS_SERVER_IO
void CheckOutstandingIO(LPDNS_SERVER_INFO lpServerInfo)
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
#endif // __LOG_DNS_SERVER_IO

enum
{
	INIT_NONE = 0,
	INIT_LOGGER = 1,
	INIT_CONFIG = 2,
	INIT_WSADATA = 4,
	INIT_SOCKET_POOL = 8
};

struct ServerParameters
{
	int Initialized;
	WSADATA WSAData;
	LPDNS_SERVER_INFO lpDnsServerInfo;
	LPWEB_SERVER_INFO lpWebServerInfo;
};

#define SetInit(_InitBit) lp->Initialized |= _InitBit

BOOL Initialize(struct ServerParameters* lp)
{
	if (lp->Initialized != INIT_NONE)
	{
		Error("/// "__FUNCTION__ " - Error: Cannot initialize twice! ///");
		return FALSE;
	}

	LoggerInitialize(); // This must always be first because all logging depend on it
	SetInit(INIT_LOGGER);

	if (!LoadConfiguration())
	{
		Error("/// Failed to load config.cfg ///");
		return FALSE;
	}

	SetInit(INIT_CONFIG);

	if (!g_Configuration.DnsServer.Enabled &&
		!g_Configuration.Proxy.Enabled &&
		!g_Configuration.Web.Enabled)
	{
		Error("/// (Failure) No service configured to run in config.cfg ///");
		return FALSE;
	}

	WSAStartup(MAKEWORD(2, 2), &lp->WSAData);
	SetInit(INIT_WSADATA);

	InitializeSocketPool();
	SetInit(INIT_SOCKET_POOL);

	if (g_Configuration.DnsServer.Enabled)
	{
		if (!(lp->lpDnsServerInfo = AllocateDnsServerInfo()))
		{
			Error("Failed to allocate DNS server info");
			return FALSE;
		}

		SOCKADDR_IN SecondaryDnsServer;
		MakeSocketAddress(
			&SecondaryDnsServer,
			g_Configuration.DnsServer.SecondaryDnsServerAddress,
			g_Configuration.DnsServer.SecondaryDnsServerPort);
		DnsServerSetSecondaryDnsServer(lp->lpDnsServerInfo, &SecondaryDnsServer);

		if (!DnsServerStart(lp->lpDnsServerInfo, g_Configuration.DnsServer.Port))
		{
			Error("Failed to start DNS server, socket error: %d - %s", GetLastError(), GetErrorMessage(GetLastError()));
			return FALSE;
		}

		Error("/// DNS Server started on port %d ///", g_Configuration.DnsServer.Port);
	}

	// Proxy and Web too...
	if (g_Configuration.Web.Enabled)
	{
		if (!(lp->lpWebServerInfo = AllocateWebServerInfo(2)))
		{
			Error("Failed to allocate Web server info");
			return FALSE;
		}

		if (!WebServerStart(lp->lpWebServerInfo, g_Configuration.Web.Port))
		{
			Error("Failed to start Web server, socket error: %d - %s", GetLastError(), GetErrorMessage(GetLastError()));
			return FALSE;
		}

		Error("/// Web Server started on port %d ///", g_Configuration.Web.Port);
	}

	return TRUE;
}

void Uninitalize(struct ServerParameters* lp)
{
	if (lp->lpDnsServerInfo)
		DestroyDnsServerInfo(lp->lpDnsServerInfo);

	if (lp->lpWebServerInfo)
		DestroyWebServerInfo(lp->lpWebServerInfo);

	if (lp->Initialized & INIT_SOCKET_POOL)
		DestroySocketPool();

	if (lp->Initialized & INIT_WSADATA)
		WSACleanup();

	if (lp->Initialized & INIT_LOGGER)
		LoggerDestroy();
}

int main(int argc, char* argv[])
{
	SetConsoleTitle(L"DNS Server Test");

	struct ServerParameters serverParameters;
	ZeroMemory(&serverParameters, sizeof(struct ServerParameters));

	if (!Initialize(&serverParameters))
	{
		Uninitalize(&serverParameters);
		return 1;
	}

	while (1)
	{
		if (_kbhit() && _getch() == VK_ESCAPE)
			break;

#ifdef __LOG_DNS_SERVER_IO
		CheckOutstandingIO(serverParameters.lpDnsServerInfo);
#endif // __LOG_DNS_SERVER_IO

		if (g_Configuration.DnsServer.Enabled)
			DnsRequestTimeoutProcess(serverParameters.lpDnsServerInfo->lpRequestTimeoutHandler);

		// (Maintenance) Fill up the socket pool
		SocketPoolFill();

		Sleep(100);
	}

	Error("/// STOPPING SERVER ///");

	Uninitalize(&serverParameters);
	return 0;
}