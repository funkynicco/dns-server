#include "StdAfx.h"
#include "ClusterPacket.h"

namespace network::cluster
{
    ClusterPacketData ClusterPacketData::Initialize(PacketHeader header, uint16_t sequence)
    {
        ClusterPacketData data;
        InitializePacketData(&data, header, sequence);
        return data;
    }

    void ClusterPacket::Release()
    {
        ClusterPacketPool::Release(this);
    }

    void InitializePacketData(ClusterPacketData* data, PacketHeader header, uint16_t sequence)
    {
        memcpy(data->Signature, PACKET_SIGNATURE, sizeof(data->Signature));
        data->Header = header;
        data->Sequence = sequence;
        data->DataSize = 0;
    }

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////

    /*
     * Pooled memory allocated here is deliberately not deallocated as when
     * application closes the system will clean up the memory.
     */

    const size_t ContiguousPacketsToAllocate = 8;

    std::mutex ClusterPacketPool::s_mutex;
    ClusterPacket* ClusterPacketPool::s_pool = nullptr;

    ClusterPacket* ClusterPacketPool::Acquire()
    {
        std::lock_guard g(s_mutex);
        if (s_pool == nullptr)
        {
            // allocate packets in contiguous memory
            // (this memory is only deallocated on application close)
            auto packets = (ClusterPacket*)malloc(sizeof(ClusterPacket) * ContiguousPacketsToAllocate);
            for (size_t i = 0; i < ContiguousPacketsToAllocate; i++)
            {
                auto current = &packets[i];
                new(current) ClusterPacket();
                current->next = s_pool;
                s_pool = current;
            }
        }

        auto packet = s_pool;
        s_pool = s_pool->next;
        return packet;
    }

    void ClusterPacketPool::Release(ClusterPacket* packet)
    {
        std::lock_guard g(s_mutex);
        packet->next = s_pool;
        s_pool = packet;
    }
}
