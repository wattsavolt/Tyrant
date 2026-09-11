#pragma once

#include "TaskPool.h"

namespace tyr
{
    TYR_THREADLOCAL uint TaskPool::s_LocalFreeCache[TaskPool::c_LocalCacheSize] = {};
    TYR_THREADLOCAL uint TaskPool::s_LocalFreeCount = 0;

    TaskPool::TaskPool()
    {
        TYR_ASSERT(!s_Initialized && "Only one TaskPool instance allowed");
        s_Initialized = true;
    }

    TaskPool::~TaskPool()
    {
        TYR_ASSERT(m_ObjectCount.load(std::memory_order_acquire) == 0);
        s_Initialized = false;
    }

    TaskPool::Handle TaskPool::Create()
    {
        const uint index = m_FreeList.Pop();
        TYR_ASSERT(index != AtomicFreeList<c_Capacity>::c_InvalidIndex);

        m_ObjectCount.fetch_add(1, std::memory_order_relaxed);

        return MakeHandle(index, m_Generations[index]);
    }

    void TaskPool::Delete(Handle h)
    {
        TYR_ASSERT(IsValid(h));

        const uint index = GetIndex(h);

        m_Generations[index]++;

        s_LocalFreeCache[s_LocalFreeCount++] = index;

        if (s_LocalFreeCount == c_LocalCacheSize)
        {
            FlushLocalCache();
        }

        m_ObjectCount.fetch_sub(1, std::memory_order_relaxed);
    }

    bool TaskPool::IsValid(Handle h) const
    {
        const uint index = GetIndex(h);
        return index < c_Capacity && m_Generations[index] == GetGeneration(h);
    }

    Task& TaskPool::operator[](Handle h)
    {
        TYR_ASSERT(IsValid(h));
        return m_Pool[GetIndex(h)];
    }

    const Task& TaskPool::operator[](Handle h) const
    {
        TYR_ASSERT(IsValid(h));
        return m_Pool[GetIndex(h)];
    }

    TaskPool::Handle TaskPool::MakeHandle(uint index, uint generation)
    {
        return (Handle(generation) << 32) | index;
    }

    uint TaskPool::GetIndex(Handle h)
    {
        return uint(h);
    }

    uint TaskPool::GetGeneration(Handle h)
    {
        return uint(h >> 32);
    }

    void TaskPool::FlushLocalCache()
    {
        for (uint i = 0; i < s_LocalFreeCount; ++i)
        {
            m_FreeList.Push(s_LocalFreeCache[i]);
        }

        s_LocalFreeCount = 0;
    }
}
