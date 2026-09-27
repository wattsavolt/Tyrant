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

            if (next == m_ReadIndex.load(std::memory_order_acquire))
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
            const uint readIndex = m_ReadIndex.load(std::memory_order_relaxed);

            if (readIndex == writeIndex)
            {
                return std::nullopt;
            }

            T value = m_Slots[readIndex];
            m_ReadIndex.store((readIndex + 1) & c_Mask, std::memory_order_release);

            return value;
        }

        Optional<T> ReadOnly() const
        {
            const uint writeIndex = m_WriteIndex.load(std::memory_order_acquire);
            const uint readIndex = m_ReadIndex.load(std::memory_order_relaxed);

            if (readIndex == writeIndex)
            {
                return std::nullopt;
            }

            return m_Slots[readIndex];
        }

        void Pop()
        {
            const uint readIndex = m_ReadIndex.load(std::memory_order_relaxed);
            TYR_ASSERT(readIndex != m_WriteIndex.load(std::memory_order_acquire));
            m_ReadIndex.store((readIndex + 1) & c_Mask, std::memory_order_release);
        }

        bool IsEmpty() const
        {
            return m_ReadIndex.load(std::memory_order_acquire) == m_WriteIndex.load(std::memory_order_acquire);
        }

        bool IsFull() const
        {
            const uint writeIndex = m_WriteIndex.load(std::memory_order_acquire);
            return ((writeIndex + 1) & c_Mask) == m_ReadIndex.load(std::memory_order_acquire);
        }

    private:

        static constexpr uint c_Mask = Capacity - 1;

        alignas(64) T m_Slots[Capacity];

        // Producer cache line
        alignas(64) Atomic<uint> m_WriteIndex;

        // Consumer cache line. Atomic (relaxed is enough for the consumer's own reads of it,
        // since it's the only thread that ever writes it) so the producer's cross-thread
        // reads of it in Enqueue/IsEmpty/IsFull are well-defined instead of a data race on a
        // plain uint - costs nothing extra since a relaxed/acquire load or release store on a
        // naturally-aligned uint compiles to the same plain instruction as a non-atomic one.
        alignas(64) Atomic<uint> m_ReadIndex;
    };
}

        