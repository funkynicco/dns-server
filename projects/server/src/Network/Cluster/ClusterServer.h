#pragma once

#include "Network/UdpServer.h"
#include "Network/NetUtils.h"

#include "ClusterPacket.h"
#include "ClusterServerClient.h"

namespace network::cluster
{
    class ClusterServer : public UdpServer
    {
        static constexpr int MAX_EPOLL_EVENTS = 64;

    public:
        ClusterServer(const Configuration& configuration, ILogger* logger);
        virtual ~ClusterServer() override;

        IPv4Filter& GetIPv4Filter() { return m_ipv4filter; }
        void SetMyAddress(in_addr addr) { m_myaddr = addr; }

        virtual void HandlePacket(sockaddr_in from, const char* data, size_t len) override;
        void ProcessPacket(ClusterPacket* packet);
        void ProcessPacketQueue();
        void Process();
        void ReceiveAllClients();

        void Send(sockaddr_in to, const ClusterPacketData* packet_data);
        void Broadcast(const ClusterPacketData* packet_data);

    protected:
        void OnJoinRequest(ClusterPacket* packet);
        void OnJoinResponse(ClusterPacket* packet);
        void OnHandshakeRequest(ClusterPacket* packet);

    private:
        const Configuration& m_configuration;
        ILogger* m_logger;

        IPv4Filter m_ipv4filter;
        in_addr m_myaddr;
        sockaddr_in m_broadcastaddr;

        bool m_joined;
        time_t m_tNextBroadcastJoin;
        time_t m_tTimeoutJoining;

        typedef void (ClusterServer::* TPacketHandler)(ClusterPacket* packet);
        TPacketHandler m_packets[128];
        std::unordered_map<uint32_t, ClusterServerClient*> m_clients;

        std::queue<ClusterPacket*> m_receive_queue;
        std::mutex m_receive_queue_lock;

        int m_epollfd;

        static bool ValidateClusterPacketData(size_t raw_packet_length, const ClusterPacketData* packet_data);
    };
}
