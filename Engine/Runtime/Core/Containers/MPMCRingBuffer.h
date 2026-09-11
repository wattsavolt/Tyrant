#pragma once

#include "Base/Base.h"
#include "Memory/Allocation.h"
#include "Threading/ThreadTypes.h"

namespace tyr
{
    // Lock-free multi-producer, multi-consumer (MPMC) ring buffer
    // Based on Dmitry Vyukov's bounded MPMC queue
    template<typename T, uint Capacity>
    class MPMCRingBuffer
    {
    public:
        MPMCRingBuffer()
            : m_EnqueuePos(0)
            , m_DequeuePos(0)
        {
            TYR_STATIC_ASSERT((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");

            for (uint i = 0; i < Capacity; ++i)
            {
                m_Slots[i].sequence.store(i, std::memory_order_relaxed);
            }
        }

        bool Enqueue(const T& item)
        {
            Slot* slot;
            uint pos = m_EnqueuePos.load(std::memory_order_relaxed);

            for (;;)
            {
                slot = &m_Slots[pos & c_Mask];

                uint seq = slot->sequence.load(std::memory_order_acquire);
                const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

                if (diff == 0)
                {
                    if (m_EnqueuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    {
                        break;
                    }
                }
                else if (diff < 0)
                {
                    return false; // queue full
                }
                else
                {
                    pos = m_EnqueuePos.load(std::memory_order_relaxed);
                }
            }

            slot->value = item;
            slot->sequence.store(pos + 1, std::memory_order_release);

            return true;
        }

        Optional<T> Dequeue()
        {
            Slot* slot;
            uint pos = m_DequeuePos.load(std::memory_order_relaxed);

            for (;;)
            {
                slot = &m_Slots[pos & c_Mask];

                const uint seq = slot->sequence.load(std::memory_order_acquire);
                const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

                if (diff == 0)
                {
                    if (m_DequeuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    {
                        break;
                    }
                }
                else if (diff < 0)
                {
                    return std::nullopt; // queue empty
                }
                else
                {
                    pos = m_DequeuePos.load(std::memory_order_relaxed);
                }
            }

            T value = slot->value;
            slot->sequence.store(pos + Capacity, std::memory_order_release);

            return value;
        }

    private:

        struct Slot
        {
            Atomic<uint> sequence;
            T value;
        };

        static constexpr uint c_Mask = Capacity - 1;

        Slot m_Slots[Capacity];

        alignas(64) Atomic<uint> m_EnqueuePos;
        alignas(64) Atomic<uint> m_DequeuePos;
    };
}

        