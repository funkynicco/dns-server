#pragma once

#include "Network/UdpServer.h"
#include "Network/NetUtils.h"

namespace network::dns
{
    class DnsServer final : public UdpServer
    {
    public:
        DnsServer() = default;        

        virtual void HandlePacket(sockaddr_in from, const char* data, size_t len) override;
        void Process();

    private:
    };
}
