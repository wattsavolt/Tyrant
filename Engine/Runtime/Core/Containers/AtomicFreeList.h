#pragma once

#include "Base/Base.h"
#include "Memory/Allocation.h"
#include "Threading/ThreadTypes.h"

namespace tyr
{
    template<uint Capacity>
    class AtomicFreeList final
    {
    public:
        static constexpr uint c_InvalidIndex = UINT32_MAX;

        AtomicFreeList()
        {
            for (uint i = 0; i < Capacity - 1; ++i)
            {
                m_Nodes[i].next = i + 1;
            }

            m_Nodes[Capacity - 1].next = c_InvalidIndex;

            m_Head.store(0, std::memory_order_release);
        }

        uint Pop()
        {
            uint64 head = m_Head.load(std::memory_order_acquire);

            for (;;)
            {
                const uint index = GetIndex(head);

                if (index == c_InvalidIndex)
                {
                    break;
                }

                const uint next = m_Nodes[index].next;
                const uint tag = GetTag(head);

                const uint64 newHead = Pack(next, tag + 1);

                if (m_Head.compare_exchange_weak(head, newHead, std::memory_order_acq_rel, std::memory_order_acquire))
                {
                    return index;
                }
            }

            return c_InvalidIndex;
        }

        void Push(uint index)
        {
            uint64 head = m_Head.load(std::memory_order_acquire);

            for (;;)
            {
                const uint headIndex = GetIndex(head);
                const uint tag = GetTag(head);

                m_Nodes[index].next = headIndex;

                const uint64 newHead = Pack(index, tag + 1);

                if (m_Head.compare_exchange_weak(head, newHead, std::memory_order_acq_rel, std::memory_order_acquire))
                {
                    break;
                }
            }
        }

    private:

        struct Node
        {
            uint next;
        };

        static uint64 Pack(uint index, uint tag)
        {
            return (uint64(tag) << 32) | index;
        }

        static uint GetIndex(uint64 v)
        {
            return uint(v);
        }

        static uint GetTag(uint64 v)
        {
            return uint(v >> 32);
        }

    private:
        Node m_Nodes[Capacity];
        alignas(64) Atomic<uint64> m_Head;
    };
}

        