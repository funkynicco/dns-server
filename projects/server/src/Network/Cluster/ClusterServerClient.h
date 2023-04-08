#pragma once

#include "Network/PacketSequence.h"
#include "ClusterPacket.h"

namespace network::cluster
{
    class ClusterServerClient
    {
    public:
        enum class State
        {
            Handshake,
            Connected,
        };

        ClusterServerClient(const Configuration& configuration, ILogger* logger, sockaddr_in addr);
        ~ClusterServerClient();

        void Process();
        void OnPacketReceived(const ClusterPacketData* packet_data);
        void OnHandshakeResponse(const ClusterPacketData* packet_data);
        void OnHandshakeResponseAcknowledged(const ClusterPacketData* packet_data);
        void OnPing(const ClusterPacketData* packet_data);
        void OnPong(const ClusterPacketData* packet_data);

        void Send(const ClusterPacketData* packet_data);
        void SendHandshakeRequest();
        void SendHandshakeResponse();
        void SendHandshakeResponseAcknowledged();
        void SendPing();
        void SendPong();

        void Delete(const std::string& reason)
        {
            m_delete = true;
            m_delete_reason = reason;
        }

        [[nodiscard]] bool IsDelete() const { return m_delete; }
        [[nodiscard]] const std::string& GetDeleteReason() const { return m_delete_reason; }

        [[nodiscard]] sockaddr_in GetAddress() const { return m_address; }
        void SetAddress(sockaddr_in addr) { m_address = addr; }
        [[nodiscard]] SOCKET GetSocket() const { return m_socket; }

    private:
        const Configuration& m_configuration;
        ILogger* m_logger;
        std::string m_delete_reason;
        bool m_delete;
        sockaddr_in m_address;
        SOCKET m_socket;
        PacketSequence<ClusterPacket> m_sequence_receive;
        uint16_t m_sequence_send;
        State m_state;
        typedef void (ClusterServerClient::* TPacketHandler)(const ClusterPacketData* packet_data);
        TPacketHandler m_packets[128];

        time_t m_tLastSentPing;
        time_t m_tLastActivity;
    };
}
