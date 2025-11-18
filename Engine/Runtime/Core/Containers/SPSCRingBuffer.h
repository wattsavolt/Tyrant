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
            
        }

        // Producer-side
        bool Enqueue(const T& item)
        {
            const uint nextWrite = Math::ComputeWrappedIncrement<Capacity>(m_WriteIndex);
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
        Optional<T> Read()
        {
            if (m_ReadIndex != m_WriteIndex)
            {
                T& value = m_Slots[m_ReadIndex];
                m_ReadIndex = Math::ComputeWrappedIncrement<Capacity>(m_ReadIndex);
                return value;
            }
            // Buffer is empty
            return std::nullopt;
        }

        Optional<T> ReadOnly()
        {
            if (m_ReadIndex != m_WriteIndex)
            {
                return m_Slots[m_ReadIndex];
            }
            // Buffer is empty
            return std::nullopt;
        }

        void Pop()
        {
            TYR_ASSERT(m_ReadIndex != m_WriteIndex);
            m_ReadIndex = Math::ComputeWrappedIncrement<Capacity>(m_ReadIndex);
        }

        bool IsEmpty() const
        {
            return m_ReadIndex == m_WriteIndex;
        }

        bool IsFull() const
        {
            return m_ReadIndex == Math::ComputeWrappedIncrement<Capacity>(m_WriteIndex);
        }

    private:
        T m_Slots[Capacity];

        uint m_WriteIndex; // Written only by producer
        uint m_ReadIndex;  // Written only by consumer
    };
}

        