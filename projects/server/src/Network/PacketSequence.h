#pragma once

#define MAX_PACKET_SEQUENCE 65536

namespace network
{
    template <typename T>
    class PacketSequence
    {
    public:
        typedef void (*pfDestroyItem)(T* value);

        explicit PacketSequence(pfDestroyItem pfnDestroyItem = nullptr);
        ~PacketSequence();

        PacketSequence(const PacketSequence&) = delete;
        PacketSequence(PacketSequence&&) = delete;
        PacketSequence& operator =(const PacketSequence&) = delete;
        PacketSequence& operator =(PacketSequence&&) = delete;

        void Set(uint16_t sequence, T* value);
        T* Next();
        T* Peek();
        [[nodiscard]] const T* Peek() const;
        [[nodiscard]] bool HasNext() const;

    private:
        pfDestroyItem m_pfnDestroyItem;
        uint16_t m_sequence_start;
        uint16_t m_sequence_end;
        T* m_sequence[MAX_PACKET_SEQUENCE];

        void DestroySlotAndUpdateSequenceEnd(int16_t sequence);
    };
}

#include "PacketSequence.inl"

inline void TestSequence()
{
    auto test = network::PacketSequence<int>([](auto v) { v = nullptr; });
    test.Set(0, (int*)1234);
    test.Set(2, (int*)1441);

test:
    for (;;)
    {
        const auto next = test.Next();
        if (!next)
        {
            break;
        }

        //printf("%llu\n", (uint64_t)next);
    }

    test.Set(1, (int*)592);

    goto test;
}
