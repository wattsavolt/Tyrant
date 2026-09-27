#include "ThreadPool.h"
#include "Task.h"
#include "PooledThread.h"

namespace tyr
{
    ThreadPool::ThreadPool(const ThreadPoolConfig& config, TaskScheduler* scheduler)
        : m_Threads(config.threadCount)
    {
        // PooledThread's own constructor deliberately doesn't start its OS thread - every
        // worker needs its index and back-pointers set first (Launch does that), otherwise
        // it could start stealing from / submitting to siblings before they even exist.
        for (uint i = 0; i < m_Threads.Size(); ++i)
        {
            m_Threads[i].Launch(i, this, scheduler);
        }
    }

    ThreadPool::~ThreadPool()
    {
        // Stop every worker before joining any of them, so none is destroyed (and thus
        // unsafe to Steal() from) while a sibling might still reach into its WorkerContext.
        for (uint i = 0; i < m_Threads.Size(); ++i)
        {
            m_Threads[i].RequestStop();
        }

        for (uint i = 0; i < m_Threads.Size(); ++i)
        {
            m_Threads[i].Join();
        }
    }

    uint ThreadPool::GetWorkerCount() const
    {
        return m_Threads.Size();
    }

    WorkerContext& ThreadPool::GetWorkerContext(uint index)
    {
        TYR_ASSERT(index < m_Threads.Size());
        return m_Threads[index].GetContext();
    }
}
