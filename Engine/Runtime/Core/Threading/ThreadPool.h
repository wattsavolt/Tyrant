#pragma once

#include "Base/Base.h"
#include "Containers/Array.h"
#include "ThreadTypes.h"

namespace tyr
{
    // Configuration struct for thread pool
    struct ThreadPoolConfig
    {
        uint threadCount = static_cast<uint>(Thread::hardware_concurrency());
    };

    class PooledThread;
    class ThreadPool final
    {
    public:
        ThreadPool(const ThreadPoolConfig& config);
        ~ThreadPool();

        PooledThread* GetAvailableThread();

    private:
        Array<PooledThread> m_Threads;
        Mutex m_Mutex;
    };

}