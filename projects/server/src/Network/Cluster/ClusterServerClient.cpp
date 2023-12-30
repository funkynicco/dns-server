#include "StdAfx.h"
#include "ClusterServerClient.h"

#include "Network/NetUtils.h"

#define BindPacket(header, func) m_packets[(int)(header)] = &ClusterServerClient::func

namespace network::cluster
{
    ClusterServerClient::ClusterServerClient(const sockaddr_in addr)
    {
        m_delete = false;
        m_address = addr;
        m_sequence_send = 0;
        m_state = State::Handshake;

        m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        
        // bind functions
        memset(m_packets, 0, sizeof(m_packets));
        BindPacket(PacketHeader::Ping, OnPing);
        BindPacket(PacketHeader::Pong, OnPong);
        BindPacket(PacketHeader::HandshakeResponse, OnHandshakeResponse);
        BindPacket(PacketHeader::HandshakeResponseAcknowledged, OnHandshakeResponseAcknowledged);

        m_tLastSentPing = time(nullptr);
        m_tLastActivity = time(nullptr);
    }

    ClusterServerClient::~ClusterServerClient()
    {
        if (m_socket != INVALID_SOCKET)
        {
            closesocket(m_socket);
        }

#ifdef _DEBUG
        m_delete_reason = "CLASS_DELETED";
        memset(&m_delete, 0xcd, sizeof(m_delete));
        memset(&m_address, 0xcd, sizeof(m_address));
        memset(&m_socket, 0xcd, sizeof(m_socket));
        memset(&m_sequence_send, 0xcd, sizeof(m_sequence_send));
        memset(&m_state, 0xcd, sizeof(m_state));
        memset(m_packets, 0xcd, sizeof(m_packets));
        memset(&m_tLastSentPing, 0xcd, sizeof(m_tLastSentPing));
        memset(&m_tLastActivity, 0xcd, sizeof(m_tLastActivity));
#endif
    }

    void ClusterServerClient::Process()
    {
        const auto now = time(nullptr);

        if (m_state == State::Connected)
        {
            if (now - m_tLastSentPing > 5)
            {
                m_tLastSentPing = now;
                SendPing();
            }
        }

        if (now - m_tLastActivity > 30)
        {
            Delete("No network activity for 30 seconds");
        }
    }

    void ClusterServerClient::OnPacketReceived(const ClusterPacketData* packet_data)
    {
        const auto logger = Globals::Get<ILogger>();
        
        if (packet_data->Header != PacketHeader::Ping &&
            packet_data->Header != PacketHeader::Pong)
        {
            logger->Log(LogType::Debug, "ClusterServer", nl::String::Format(
                "[Client] Received {} (seq: {}) from {}:{}",
                PacketHeaderToString(packet_data->Header).c_str(),
                packet_data->Sequence,
                AddrToStr(m_address.sin_addr).c_str(),
                ntohs(m_address.sin_port)
            ));
        }

        m_tLastActivity = time(nullptr);

        int pid = (int)packet_data->Header;
        if (pid < 0 ||
            pid >= ARRAYSIZE(m_packets))
        {
            return;
        }

        const auto handler = m_packets[pid];
        if (!handler)
        {
            return;
        }

        (this->*handler)(packet_data);
    }

    void ClusterServerClient::OnHandshakeResponse(const ClusterPacketData* packet_data)
    {
        const auto logger = Globals::Get<ILogger>();
        
        m_state = State::Connected;
        m_tLastSentPing = time(nullptr);
        SendHandshakeResponseAcknowledged();
        
        logger->Log(LogType::Debug, "ClusterServer", nl::String::Format(
            "[{}:{}] State changed to connected!",
            AddrToStr(m_address.sin_addr).c_str(),
            ntohs(m_address.sin_port)
        ));
    }

    void ClusterServerClient::OnHandshakeResponseAcknowledged(const ClusterPacketData* packet_data)
    {
        const auto logger = Globals::Get<ILogger>();
        
        m_state = State::Connected;
        m_tLastSentPing = time(nullptr);

        logger->Log(LogType::Debug, "ClusterServer", nl::String::Format(
            "[{}:{}] State changed to connected!",
            AddrToStr(m_address.sin_addr).c_str(),
            ntohs(m_address.sin_port)
        ));
    }

    void ClusterServerClient::OnPing(const ClusterPacketData* packet_data)
    {
        SendPong();
    }

    void ClusterServerClient::OnPong(const ClusterPacketData* packet_data)
    {
        // nothing needed to be done
    }

    void ClusterServerClient::Send(const ClusterPacketData* packet_data)
    {
        const int sent = sendto(
            m_socket,
            (const char*)packet_data,
            MIN_PACKET_SIZE + packet_data->DataSize,
            0,
            (const sockaddr*)&m_address,
            sizeof(m_address)
        );

        if (sent <= 0)
        {
            Delete("Network error: sendto failed");
        }
    }

    void ClusterServerClient::SendHandshakeRequest()
    {
        const auto data = ClusterPacketData::Initialize(PacketHeader::HandshakeRequest, 0);
        Send(&data);
    }

    void ClusterServerClient::SendHandshakeResponse()
    {
        const auto data = ClusterPacketData::Initialize(PacketHeader::HandshakeResponse, 0);
        Send(&data);
    }

    void ClusterServerClient::SendHandshakeResponseAcknowledged()
    {
        const auto data = ClusterPacketData::Initialize(PacketHeader::HandshakeResponseAcknowledged, 0);
        Send(&data);
    }

    void ClusterServerClient::SendPing()
    {
        const uint16_t sequence = m_sequence_send++;

        const auto data = ClusterPacketData::Initialize(PacketHeader::Ping, sequence);
        Send(&data);
    }

    void ClusterServerClient::SendPong()
    {
        const uint16_t sequence = m_sequence_send++;

        const auto data = ClusterPacketData::Initialize(PacketHeader::Pong, sequence);
        Send(&data);
    }
}
