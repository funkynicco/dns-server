#include "StdAfx.h"
#include "DnsHosts.h"
/*
BOOL DnsHosts::TryResolve(const char* domain, LPDWORD lpdwIP)
{
    LPDNS_HOST_INFO lpHost = (LPDNS_HOST_INFO)PtrMapFind(g_ptrHostsMap, DnsHostsHashKey(domain));
    if (!lpHost)
        return FALSE;

    *lpdwIP = lpHost->dwIP;
    return TRUE;
}*/