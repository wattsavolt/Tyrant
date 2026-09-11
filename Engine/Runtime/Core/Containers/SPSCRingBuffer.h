#pragma once

#include "Base/Base.h"
#include "Memory/Allocation.h"
#include "Threading/ThreadTypes.h"

namespace tyr
{
    template<typename T, uint Capacity>
    class SPSCRingBuffer
    {
    public:
        SPSCRingBuffer()
            : m_WriteIndex(0)
            , m_ReadIndex(0)
        {
            TYR_STATIC_ASSERT((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");
        }

        // Producer thread
        bool Enqueue(const T& item)
        {
            const uint writeIndex = m_WriteIndex.load(std::memory_order_relaxed);
            const uint next = (writeIndex + 1) & c_Mask;

            if (next == m_ReadIndex)
            {
                // Buffer full
                return false;
            }

            m_Slots[writeIndex] = item;

            // Publish the write
            m_WriteIndex.store(next, std::memory_order_release);
            return true;
        }

        // Consumer thread
        Optional<T> Dequeue()
        {
            const uint writeIndex = m_WriteIndex.load(std::memory_order_acquire);

            if (m_ReadIndex == writeIndex)
            {
                return std::nullopt;
            }

            T value = m_Slots[m_ReadIndex];
            m_ReadIndex = (m_ReadIndex + 1) & c_Mask;

            return value;
        }

        Optional<T> ReadOnly() const
        {
            const uint writeIndex = m_WriteIndex.load(std::memory_order_acquire);

            if (m_ReadIndex == writeIndex)
            {
                return std::nullopt;
            }

            return m_Slots[m_ReadIndex];
        }

        void Pop()
        {
            TYR_ASSERT(m_ReadIndex != m_WriteIndex.load(std::memory_order_acquire));
            m_ReadIndex = (m_ReadIndex + 1) & c_Mask;
        }

        bool IsEmpty() const
        {
            return m_ReadIndex == m_WriteIndex.load(std::memory_order_acquire);
        }

        bool IsFull() const
        {
            const uint writeIndex = m_WriteIndex.load(std::memory_order_acquire);
            return ((writeIndex + 1) & c_Mask) == m_ReadIndex;
        }

    private:

        static constexpr uint c_Mask = Capacity - 1;

        alignas(64) T m_Slots[Capacity];

        // Producer cache line
        alignas(64) Atomic<uint> m_WriteIndex;

        // Consumer cache line
        alignas(64) uint m_ReadIndex;
    };
}

        