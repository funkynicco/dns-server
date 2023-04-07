#include "StdAfx.h"
#include "NetUtils.h"

#include <unordered_map>

namespace network
{
    // provide a subnet map to bits to validate and also retrieve the bits part
    using SubnetMap = std::unordered_map<std::string, uint32_t>;
    static SubnetMap s_subnet_map =
    {
        { "0.0.0.0", 0 },
        { "128.0.0.0", 1 },
        { "192.0.0.0", 2 },
        { "224.0.0.0", 3 },
        { "240.0.0.0", 4 },
        { "248.0.0.0", 5 },
        { "252.0.0.0", 6 },
        { "254.0.0.0", 7 },
        { "255.0.0.0", 8 },
        { "255.128.0.0", 9 },
        { "255.192.0.0", 10 },
        { "255.224.0.0", 11 },
        { "255.240.0.0", 12 },
        { "255.248.0.0", 13 },
        { "255.252.0.0", 14 },
        { "255.254.0.0", 15 },
        { "255.255.0.0", 16 },
        { "255.255.128.0", 17 },
        { "255.255.192.0", 18 },
        { "255.255.224.0", 19 },
        { "255.255.240.0", 20 },
        { "255.255.248.0", 21 },
        { "255.255.252.0", 22 },
        { "255.255.254.0", 23 },
        { "255.255.255.0", 24 },
        { "255.255.255.128", 25 },
        { "255.255.255.192", 26 },
        { "255.255.255.224", 27 },
        { "255.255.255.240", 28 },
        { "255.255.255.248", 29 },
        { "255.255.255.252", 30 },
        { "255.255.255.254", 31 },
        { "255.255.255.255", 32 },
    };

    IPv4Filter& IPv4Filter::AddRule(const char* address, const char* subnet)
    {
        std::unique_lock g(m_rwlock);

        if (!subnet)
        {
            // if no subnet is provided, treat address as a single ip
            subnet = "255.255.255.255";
        }

        SubnetMap::iterator it = s_subnet_map.find(subnet);
        if (it == s_subnet_map.end())
        {
            throw Exception("The provided subnet is not valid.");
        }

        // inet_addr("127.0.0.1") & 0xff = 127 ; big endian (network order)

        int shift = 32 - it->second;
        //ntohl() // net to host
        //htonl() // host to net

        uint32_t ip_host = ntohl(inet_addr(address)) >> shift;

        auto tup = std::make_tuple(ip_host, shift);

        if (std::find(m_rules.begin(), m_rules.end(), tup) != m_rules.end())
        {
            // already added
            return *this;
        }

        m_rules.push_back(tup);
        return *this;
    }

    bool IPv4Filter::Match(uint32_t net_addr)
    {
        std::shared_lock g(m_rwlock);

        uint32_t host_addr = ntohl(net_addr);

        for (const auto& rule : m_rules)
        {
            uint32_t rule_ip = std::get<0>(rule);
            uint32_t rule_shift = std::get<1>(rule);

            if ((host_addr >> rule_shift) == rule_ip)
            {
                return true;
            }
        }

        return false;
    }

    bool IPv4Filter::Match(in_addr net_addr)
    {
        return Match((uint32_t)net_addr.s_addr);
    }

    bool IPv4Filter::Match(const char* address)
    {
        return Match(inet_addr(address));
    }

    ///////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////

    in_addr MakeAddr(const char* ip)
    {
        in_addr addr;
        addr.s_addr = inet_addr(ip);
        return addr;
    }

    sockaddr_in MakeAddr(const char* ip, int port)
    {
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(ip);
        addr.sin_port = htons((u_short)port);
        return addr;
    }

    bool AddrToStr(char* ip, size_t ip_size, uint32_t net_addr)
    {
        if (ip_size < 16)
        {
            return false;
        }

        sprintf(
            ip,
            "%d.%d.%d.%d",
            net_addr & 0xff,
            net_addr >> 8 & 0xff,
            net_addr >> 16 & 0xff,
            net_addr >> 24 & 0xff);

        return true;
    }
}
