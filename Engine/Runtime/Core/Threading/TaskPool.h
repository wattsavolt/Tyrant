#pragma once

#include "Base/Base.h"
#include "Function/Function.h"
#include "Containers/Containers.h"
#include "Memory/PoolHandle.h"
#include "Containers/AtomicFreeList.h"
#include "Task.h"

namespace tyr
{
    class TaskPool final : public INonCopyable
    {
    public:
        using Handle = uint64;

        static constexpr uint c_Capacity = 4096; // adjust as needed

        TaskPool();
        ~TaskPool();

        Handle Create();
        void Delete(Handle h);
        bool IsValid(Handle h) const;

        Task& operator[](Handle h);
        const Task& operator[](Handle h) const;

    private:

        static Handle MakeHandle(uint index, uint generation);
        static uint GetIndex(Handle h);
        static uint GetGeneration(Handle h);

        void FlushLocalCache();

    private:
        Task m_Pool[c_Capacity];
        uint m_Generations[c_Capacity] = {};

        AtomicFreeList<c_Capacity> m_FreeList;

        Atomic<uint> m_ObjectCount = 0;

        static constexpr uint c_LocalCacheSize = 64;
        thread_local static uint s_LocalFreeCache[c_LocalCacheSize];
        thread_local static uint s_LocalFreeCount;

        inline static bool s_Initialized = false;
    };
}