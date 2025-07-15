#pragma once

#include "Base/Base.h"
#include "Memory/Allocation.h"
#include "Threading/Threading.h"

namespace tyr
{
    /// Single-producer, single-consumer (SPSC) ring buffer
    ///
    /// This implementation avoids atomics because:
    /// - The producer thread *exclusively* writes `m_WriteIndex` and never reads `m_ReadIndex`
    /// - The consumer thread *exclusively* writes `m_ReadIndex` and never reads `m_WriteIndex`
    /// - Thus, values are not written to by more than one thread.
    /// - On modern CPUs, aligned 32-bit reads/writes are guaranteed to be atomic
    /// - No tearing or data races occur under strict SPSC usage
    ///
    /// This results in optimal performance with zero locking or atomic overhead.

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

        // Producer-side
        bool Enqueue(const T& item)
        {
            const uint nextWrite = (m_WriteIndex + 1) & (Capacity - 1);
            if (nextWrite == m_ReadIndex)
            {
                // Buffer is full
                return false;
            }

            m_Slots[m_WriteIndex] = item;
            m_WriteIndex = nextWrite;
            return true;
        }

        // Consumer-side
        T* Read()
        {
            if (m_ReadIndex == m_WriteIndex)
            {
                // Buffer is empty
                return nullptr;
            }

            return &m_Slots[m_ReadIndex];
        }

        void Pop()
        {
            if (m_ReadIndex != m_WriteIndex)
            {
                m_ReadIndex = (m_ReadIndex + 1) & (Capacity - 1);
            }
        }

        bool IsEmpty() const
        {
            return m_ReadIndex == m_WriteIndex;
        }

        bool IsFull() const
        {
            return ((m_WriteIndex + 1) & (Capacity - 1)) == m_ReadIndex;
        }

    private:
        T m_Slots[Capacity];

        uint m_WriteIndex; // Written only by producer
        uint m_ReadIndex;  // Written only by consumer
    };
}

        