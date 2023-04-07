#pragma once

namespace network
{
    class IPv4Filter
    {
    public:
        IPv4Filter& AddRule(const char* address, const char* subnet = nullptr);
        bool Match(uint32_t net_addr);
        bool Match(in_addr net_addr);
        bool Match(const char* address);

    private:
        std::shared_mutex m_rwlock;
        std::vector<std::tuple<uint32_t, uint32_t>> m_rules;
    };

    in_addr MakeAddr(const char* ip);
    sockaddr_in MakeAddr(const char* ip, int port);
    bool AddrToStr(char* ip, size_t ip_size, uint32_t net_addr);

    template <size_t _Size>
    inline bool AddrToStr(char(&ip)[_Size], uint32_t net_addr)
    {
        return AddrToStr(ip, _Size, net_addr);
    }

    template <size_t _Size>
    inline bool AddrToStr(char(&ip)[_Size], in_addr net_addr)
    {
        return AddrToStr(ip, _Size, net_addr.s_addr);
    }

    inline std::string AddrToStr(in_addr net_addr)
    {
        char ip[16] = {};
        AddrToStr(ip, net_addr);
        return ip;
    }

    inline std::string AddrToStr(uint32_t net_addr)
    {
        char ip[16] = {};
        AddrToStr(ip, net_addr);
        return ip;
    }
}