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

        // Returns task slots this thread has freed via Delete() back to the shared pool.
        // Frees are batched per-thread (see s_LocalFreeCache below) to keep contention off
        // the shared free list, which means a thread that deletes fewer than
        // c_LocalCacheSize tasks never flushes automatically and those slots stay stuck.
        // Worker threads flush this on their own while idle; any other thread that deletes
        // tasks (e.g. main) needs to call this periodically itself - TaskScheduler exposes
        // it as FlushCurrentThreadCache() for that purpose.
        void FlushCurrentThreadCache();

    private:

        static Handle MakeHandle(uint index, uint generation);
        static uint GetIndex(Handle h);
        static uint GetGeneration(Handle h);

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