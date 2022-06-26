#include "StdAfx.h"

#include "Bootstrapper\Bootstrapper.h"

#ifdef __LOG_DNS_SERVER_IO
void CheckOutstandingIO(LPDNS_SERVER_INFO lpServerInfo)
{
    EnterCriticalSection(&lpServerInfo->csStats);
    DWORD dwPendingWSARecvFrom = lpServerInfo->PendingWSARecvFrom.dwElementNum;
    DWORD dwPendingWSASendTo = lpServerInfo->PendingWSASendTo.dwElementNum;
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

BOOL WINAPI HandlerRoutine(DWORD dwCtrlType);
BOOL g_bQueryShutdown = FALSE;

enum
{
    INIT_NONE = 0,
    INIT_LOGGER = 1,
    INIT_CONFIG = 2,
    INIT_WSADATA = 4,
    INIT_SOCKET_POOL = 8,
    INIT_LOAD_HOSTS = 16
};

struct ServerParameters
{
    int Initialized;
    WSADATA WSAData;
    LPDNS_SERVER_INFO lpDnsServerInfo;
    LPWEB_SERVER_INFO lpWebServerInfo;
    Bootstrapper* pBootstrapper;
};

#define SetInit(_InitBit) lp->Initialized |= _InitBit

BOOL Initialize(struct ServerParameters* lp, BOOL bService)
{
    if (lp->Initialized != INIT_NONE)
    {
        Error("/// " __FUNCTION__ " - Error: Cannot initialize twice! ///");
        return FALSE;
    }

    WSAStartup(MAKEWORD(2, 2), &lp->WSAData);
    SetInit(INIT_WSADATA);

    ConsoleInitialize();
    LoggerInitialize(); // This must always be first (after WSAStartup) because all logging depend on it
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

    // initialize bootstrapper
    if (bService)
    {
        lp->pBootstrapper = new Bootstrapper;
        if (!lp->pBootstrapper->Connect())
            return FALSE;
    }

    InitializeSocketPool();
    SetInit(INIT_SOCKET_POOL);

    if (!g_dnsHosts.Load())
        return FALSE;
    SetInit(INIT_LOAD_HOSTS);

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

        if (!DnsServerStart(lp->lpDnsServerInfo))
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

        if (!WebServerStart(lp->lpWebServerInfo))
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

    if (lp->pBootstrapper)
        delete lp->pBootstrapper;

    if (lp->Initialized & INIT_LOGGER)
    {
        LoggerDestroy();
        ConsoleDestroy();
    }

    if (lp->Initialized & INIT_WSADATA)
        WSACleanup();
}

int main(int argc, char* argv[])
{
    SetConsoleTitle(L"DNS Server Test");
    InitializeErrorDescriptionTable();
    SetConsoleCtrlHandler(HandlerRoutine, TRUE);

    BOOL bService = FALSE;
    BOOL bBootstrapTest = FALSE;

    for (int i = 0; i < argc; ++i)
    {
        if (_strcmpi(argv[i], "--service") == 0)
        {
            bService = TRUE;
        }
        else if (_strcmpi(argv[i], "--bootstrap-test") == 0)
        {
            bBootstrapTest = TRUE;
        }
    }

    if (bBootstrapTest)
    {
        int RunBootstrapTest();
        return RunBootstrapTest();
    }

    DeleteFile(L"dns_packet.txt");

    struct ServerParameters serverParameters;
    ZeroMemory(&serverParameters, sizeof(struct ServerParameters));

    if (!Initialize(&serverParameters, bService))
    {
        Uninitalize(&serverParameters);
        return 1;
    }

    g_dnsStatistics.Startup();

    time_t tmNextCheckDb = time(NULL) + 15;

    while (!g_bQueryShutdown)
    {
        time_t tmNow = time(NULL);

#ifdef __LOG_DNS_SERVER_IO
        CheckOutstandingIO(serverParameters.lpDnsServerInfo);
#endif // __LOG_DNS_SERVER_IO

        if (g_Configuration.DnsServer.Enabled)
            DnsRequestTimeoutProcess(serverParameters.lpDnsServerInfo->lpRequestTimeoutHandler);

        // (Maintenance) Fill up the socket pool
        SocketPoolFill();

        if (g_Configuration.SQL.Enabled &&
            tmNow >= tmNextCheckDb)
        {
            g_dnsHosts.PollReload();
            tmNextCheckDb = time(NULL) + 15;
        }

        if (serverParameters.pBootstrapper)
        {
            if (serverParameters.pBootstrapper->WaitForExit(100))
                break;
        }
        else
            Sleep(100);
    }

    Error("/// STOPPING SERVER ///");

    Uninitalize(&serverParameters);
    return 0;
}

static BOOL WINAPI HandlerRoutine(DWORD dwCtrlType)
{
    switch (dwCtrlType)
    {
    case CTRL_C_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        g_bQueryShutdown = TRUE;
        return TRUE;
    }

    return FALSE;
}