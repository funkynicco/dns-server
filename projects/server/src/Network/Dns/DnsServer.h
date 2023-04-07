#pragma once

#include "Network/UdpServer.h"
#include "Network/NetUtils.h"

namespace network::dns
{
    class DnsServer : public UdpServer
    {
    public:
        DnsServer(const Configuration& configuration, ILogger* logger);

        virtual void HandlePacket(sockaddr_in from, const char* data, size_t len) override;
        void Process();

    private:
        const Configuration& m_configuration;
        ILogger* m_logger;
    };
}
