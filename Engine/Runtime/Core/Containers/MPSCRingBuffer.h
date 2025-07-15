#pragma once

#include "Base/Base.h"
#include "Memory/Allocation.h"
#include "Threading/Threading.h"

namespace tyr
{
    // Lock-free multi-producer, single-consumer (MPSC) ring buffer
    // Useful when there is frequent writes from different threads
    template<typename T, uint Capacity>
    class MPSCRingBuffer
    {
    public:
        MPSCRingBuffer()
            : m_WriteIndex(0)
            , m_ReadIndex(0)
        {
            TYR_STATIC_ASSERT(((Capacity & (Capacity - 1)) == 0), "Capacity must be a power of 2");
        }

        bool Enqueue(const T& item)
        {
            const uint index = m_WriteIndex.fetch_add(1, std::memory_order_relaxed);
            const uint slot = index & (Capacity - 1);

            if (m_Slots[slot].ready.load(std::memory_order_acquire))
            {
                TYR_ASSERT(false);
                return false;
            }

            m_Slots[slot].value = item;
            m_Slots[slot].ready.store(true, std::memory_order_release);
            return true;
        }

        Optional<T> Read() const
        {
            const size_t slot = m_ReadIndex & (Capacity - 1);

            if (!m_Slots[slot].ready.load(std::memory_order_acquire))
            {
                return std::nullopt;
            }

            return m_Slots[slot].value;
        }

    private:
        struct Slot
        {
            T value;
            Atomic<bool> ready{ false };
        };

        Slot m_Slots[Capacity];

        Atomic<uint> m_WriteIndex;
        uint m_ReadIndex;
    };
}

        