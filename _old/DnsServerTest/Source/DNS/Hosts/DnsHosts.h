#pragma once

#include <unordered_map>
#include <string>
using namespace std;

#include <sqlext.h>

typedef struct _DNS_HOST_INFO
{
    char Domain[128];
    DWORD dwIP;

    struct _DNS_HOST_INFO* next;

} DNS_HOST_INFO, *LPDNS_HOST_INFO;

class DnsHosts
{
public:
    DnsHosts();
    virtual ~DnsHosts();
    BOOL Load();
    void PollReload();

    inline BOOL TryResolve(const char* domain, LPDWORD lpdwIP)
    {
        AcquireSRWLockShared(&m_lock);
        unordered_map<string, LPDNS_HOST_INFO>::const_iterator it = m_hosts.find(domain);
        if (it != m_hosts.cend())
        {
            *lpdwIP = it->second->dwIP;
            ReleaseSRWLockShared(&m_lock);
            return TRUE;
        }

        // check wildcard domains ...

        for (const auto& it : m_wildcardHosts)
        {
            if (WildCompare(it.Domain, domain))
            {
                *lpdwIP = it.dwIP;
                ReleaseSRWLockShared(&m_lock);
                return TRUE;
            }
        }

        ReleaseSRWLockShared(&m_lock);
        return FALSE;
    }

private:
    void GetConnectionString(char* pbuf, size_t bufSize);
    unordered_map<string, LPDNS_HOST_INFO> m_hosts;
    vector<DNS_HOST_INFO> m_wildcardHosts;

    SRWLOCK m_lock;

    time_t m_tmDateChanged;
};

extern DnsHosts g_dnsHosts;