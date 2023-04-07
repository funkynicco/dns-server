#pragma once

namespace network::cluster
{
    const char PACKET_SIGNATURE[] = "CDNS";
    const size_t MAX_PACKET_DATA_SIZE = 1024;
    const size_t MIN_PACKET_SIZE = 9; // signature, header, sequence and datasize variables

    enum class PacketHeader : uint8_t
    {
        Ping = 1,
        Pong,
        JoinRequest,
        JoinResponse,
        HandshakeRequest,
        HandshakeResponse,
        HandshakeResponseAcknowledged,
        AnnounceMemberJoin,
        AnnounceMemberLeave,
    };

    inline std::string PacketHeaderToString(PacketHeader header)
    {
        switch (header)
        {
        case PacketHeader::Ping: return "Ping";
        case PacketHeader::Pong: return "Pong";
        case PacketHeader::JoinRequest: return "JoinRequest";
        case PacketHeader::JoinResponse: return "JoinResponse";
        case PacketHeader::HandshakeRequest: return "HandshakeRequest";
        case PacketHeader::HandshakeResponse: return "HandshakeResponse";
        case PacketHeader::HandshakeResponseAcknowledged: return "HandshakeResponseAcknowledged";
        case PacketHeader::AnnounceMemberJoin: return "AnnounceMemberJoin";
        case PacketHeader::AnnounceMemberLeave: return "AnnounceMemberLeave";
        }

        return "??";
    }

#pragma pack(push, 1)
    struct ClusterPacketData
    {
        char Signature[4];      // 4b
        PacketHeader Header;    // 1b
        uint16_t Sequence;      // 2b
        uint16_t DataSize;      // 2b
        // offset 9b (add together above bytes)

        uint8_t Data[MAX_PACKET_DATA_SIZE]; // 1024b
        // size 1033

        static ClusterPacketData Initialize(PacketHeader header, uint16_t sequence);
    };
#pragma pack(pop)

    struct ClusterPacket
    {
        // container of data, release, etc...
        ClusterPacket* next;
        sockaddr_in From;
        ClusterPacketData Data;

        void Release();
    };

    static_assert(
        sizeof(ClusterPacketData::Signature) == sizeof(PACKET_SIGNATURE) - 1,
        "Signature in ClusterPacket is not same size as PACKET_SIGNATURE");

    static_assert(
        sizeof(ClusterPacketData) - sizeof(ClusterPacketData::Data) == MIN_PACKET_SIZE,
        "The ClusterPacket size without Data does not match expected MIN_PACKET_SIZE");

    static_assert(
        sizeof(ClusterPacketData) <= 1500,
        "The ClusterPacket size must be maximum 1500 bytes MTU");

    void InitializePacketData(ClusterPacketData* data, PacketHeader header, uint16_t sequence);

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////

    class ClusterPacketPool
    {
    public:
        static ClusterPacket* Acquire();
        static void Release(ClusterPacket* packet);

    private:
        static std::mutex s_mutex;
        static ClusterPacket* s_pool;
    };
}