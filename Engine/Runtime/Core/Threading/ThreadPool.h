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
    class TaskScheduler;
    struct WorkerContext;

    class ThreadPool final
    {
    public:
        ThreadPool(const ThreadPoolConfig& config, TaskScheduler* scheduler);
        ~ThreadPool();

        uint GetWorkerCount() const;
        WorkerContext& GetWorkerContext(uint index);

    private:
        Array<PooledThread> m_Threads;
    };

}
