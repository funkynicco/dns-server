#pragma once

namespace network
{
    template <typename T>
    inline PacketSequence<T>::PacketSequence(pfDestroyItem pfnDestroyItem) :
        m_pfnDestroyItem(pfnDestroyItem),
        m_sequence_start(0),
        m_sequence_end(0)
    {
        memset(m_sequence, 0, sizeof(m_sequence));
    }

    template <typename T>
    inline PacketSequence<T>::~PacketSequence()
    {
        if (m_pfnDestroyItem)
        {
            for (size_t i = 0; i < MAX_PACKET_SEQUENCE; i++)
            {
                if (m_sequence[i])
                {
                    m_pfnDestroyItem(m_sequence[i]);
                }
            }
        }
    }

    template <typename T>
    inline void PacketSequence<T>::Set(uint16_t sequence, T* value)
    {
        DestroySlotAndUpdateSequenceEnd(sequence);
        m_sequence[sequence] = value;
    }

    template <typename T>
    inline T* PacketSequence<T>::Next()
    {
        T* value = m_sequence[m_sequence_start];
        if (value)
        {
            m_sequence[m_sequence_start] = nullptr;

            if (m_sequence_start == m_sequence_end)
            {
                m_sequence_end = ++m_sequence_start;
            }
            else
            {
                ++m_sequence_start;
            }
        }

        return value;
    }

    template <typename T>
    inline T* PacketSequence<T>::Peek()
    {
        return m_sequence[m_sequence_start];
    }
    
    template <typename T>
    inline const T* PacketSequence<T>::Peek() const
    {
        return m_sequence[m_sequence_start];
    }
    
    template <typename T>
    inline bool PacketSequence<T>::HasNext() const
    {
        return m_sequence[m_sequence_start] != nullptr;
    }

    template <typename T>
    inline void PacketSequence<T>::DestroySlotAndUpdateSequenceEnd(int16_t sequence)
    {
        auto& slot = m_sequence[sequence];

        if (slot)
        {
            if (m_pfnDestroyItem)
            {
                m_pfnDestroyItem(slot);
            }

            slot = nullptr;
        }

        // end is inclusive

        if (m_sequence_start <= m_sequence_end)
        {
            // start is less than end, normal array
            if (sequence < m_sequence_start ||
                sequence > m_sequence_end)
            {
                // outside the range, must update end
                m_sequence_end = sequence;
            }
        }
        else
        {
            // start is higher than end, the start has wrapped around
            if (sequence < m_sequence_start &&
                sequence > m_sequence_end)
            {
                // outside the range, must update end
                m_sequence_end = sequence;
            }
        }
    }
}
