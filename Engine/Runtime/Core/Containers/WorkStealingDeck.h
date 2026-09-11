#pragma once

#include "Base/Base.h"
#include "Memory/Allocation.h"
#include "Threading/ThreadTypes.h"

namespace tyr
{
    // Lock-free work-stealing deque (Chase–Lev)
    // Single owner thread pushes/pops, other threads may steal
    template<typename T, uint Capacity>
    class WorkStealingDeque
    {
    public:
        WorkStealingDeque()
            : m_Top(0)
            , m_Bottom(0)
        {
            TYR_STATIC_ASSERT((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");
        }

        bool Push(const T& item)
        {
            const uint bottom = m_Bottom.load(std::memory_order_relaxed);
            const uint top = m_Top.load(std::memory_order_acquire);

            if (bottom - top >= Capacity)
            {
                TYR_ASSERT(false);
                return false;
            }

            m_Buffer[bottom & c_Mask] = item;

            std::atomic_thread_fence(std::memory_order_release);

            m_Bottom.store(bottom + 1, std::memory_order_relaxed);

            return true;
        }

        Optional<T> Pop()
        {
            const uint bottom = m_Bottom.load(std::memory_order_relaxed) - 1;
            m_Bottom.store(bottom, std::memory_order_relaxed);

            std::atomic_thread_fence(std::memory_order_seq_cst);

            const uint top = m_Top.load(std::memory_order_relaxed);

            if (top > bottom)
            {
                m_Bottom.store(top, std::memory_order_relaxed);
                return std::nullopt;
            }

            const T item = m_Buffer[bottom & c_Mask];

            if (top != bottom)
            {
                return item;
            }

            if (!m_Top.compare_exchange_strong(top, top + 1, std::memory_order_seq_cst))
            {
                m_Bottom.store(top + 1, std::memory_order_relaxed);
                return std::nullopt;
            }

            m_Bottom.store(top + 1, std::memory_order_relaxed);

            return item;
        }

        Optional<T> Steal()
        {
            const uint top = m_Top.load(std::memory_order_acquire);

            std::atomic_thread_fence(std::memory_order_seq_cst);

            const uint bottom = m_Bottom.load(std::memory_order_acquire);

            if (top >= bottom)
                return std::nullopt;

            const T item = m_Buffer[top & c_Mask];

            if (!m_Top.compare_exchange_strong(top, top + 1, std::memory_order_seq_cst))
                return std::nullopt;

            return item;
        }

    private:

        static constexpr uint c_Mask = Capacity - 1;

        T m_Buffer[Capacity];

        alignas(64) Atomic<uint> m_Top;
        alignas(64) Atomic<uint> m_Bottom;
    };
}

        