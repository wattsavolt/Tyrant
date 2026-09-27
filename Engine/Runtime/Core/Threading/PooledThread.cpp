#include "PooledThread.h"
#include "ThreadPool.h"
#include "TaskScheduler.h"
#include "TaskPool.h"
#include "Task.h"
#include "TaskUtil.h"

#include <functional>

namespace tyr
{
    namespace
    {
        // Small, fast per-thread random number generator (xorshift32), used only to pick
        // which worker to try stealing from first. It doesn't need to be high quality,
        // just cheap and different enough between threads that idle workers don't all
        // pile onto the same victim at once.
        uint32_t NextRandomUint32()
        {
            static TYR_THREADLOCAL uint32_t state = 0;
            if (state == 0)
            {
                state = static_cast<uint32_t>(std::hash<std::thread::id>{}(std::this_thread::get_id())) | 1u;
            }

            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return state;
        }

        uint RandomIndex(uint bound)
        {
            return NextRandomUint32() % bound;
        }
    }

    TYR_THREADLOCAL PooledThread* PooledThread::t_Current = nullptr;

    PooledThread::PooledThread()
    {
        // Deliberately doesn't start the thread here - see Launch().
    }

    PooledThread::~PooledThread()
    {
        RequestStop();
        Join();
    }

    void PooledThread::RequestStop()
    {
        {
            LockGuard guard(m_Mutex);
            m_Stop.store(true, std::memory_order_relaxed);
        }
        m_CV.notify_one();
    }

    void PooledThread::Join()
    {
        if (m_Thread.joinable())
        {
            m_Thread.join();
        }
    }

    void PooledThread::Launch(uint index, ThreadPool* threadPool, TaskScheduler* scheduler)
    {
        m_Context.index = index;
        m_ThreadPool = threadPool;
        m_Scheduler = scheduler;

        // Only safe to start the OS thread once every field above is set - Run() may
        // start stealing from and being stolen from by siblings immediately.
        m_Thread = Thread(&PooledThread::Run, this);
    }

    PooledThread* PooledThread::GetCurrent()
    {
        return t_Current;
    }

    bool PooledThread::TryGetNextTask(TaskID& outId)
    {
        // Own queue first - cheapest, uncontended path.
        if (Optional<TaskID> own = m_Context.queue.Pop())
        {
            outId = *own;
            return true;
        }

        // Try to steal a single task from another worker. Start from a random worker so
        // many idle threads don't all target the same victim first, then scan the rest of
        // the pool once each so a single unlucky pick doesn't get re-checked while other
        // workers with real work sitting in their queue go unchecked.
        const uint workerCount = m_ThreadPool->GetWorkerCount();
        if (workerCount > 1)
        {
            const uint start = RandomIndex(workerCount);
            for (uint i = 0; i < workerCount; ++i)
            {
                const uint workerIndex = (start + i) % workerCount;
                if (workerIndex == m_Context.index)
                {
                    continue;
                }

                if (Optional<TaskID> stolen = m_ThreadPool->GetWorkerContext(workerIndex).queue.Steal())
                {
                    outId = *stolen;
                    return true;
                }
            }
        }

        // Last resort: work submitted from a non-worker thread (e.g. main), which has no
        // queue of its own to push onto.
        return m_Scheduler->TryPopInjector(outId);
    }

    void PooledThread::Run()
    {
        t_Current = this;

        for (;;)
        {
            TaskID id;
            if (TryGetNextTask(id))
            {
                m_CurrentTaskId = id;

                Task& task = m_Scheduler->GetTaskPool()[id];
                if (TaskUtil::Run(task))
                {
                    m_Scheduler->OnTaskFinished(id);
                }

                m_CurrentTaskId = c_InvalidTaskID;
                continue;
            }

            // Only actually stop once no more reachable work is left (checked just above) -
            // abandoning a queued task here would leak it forever, since nothing else would
            // ever run or free it.
            if (m_Stop.load(std::memory_order_relaxed))
            {
                break;
            }

            // Nothing to do right now. Flush any task slots this thread has freed but not
            // yet returned to the shared pool, then back off briefly before looking again -
            // this is the one point in a worker's life that's reliably "idle enough" to do
            // that flush from (see TaskPool::FlushCurrentThreadCache).
            m_Scheduler->GetTaskPool().FlushCurrentThreadCache();

            Lock lock(m_Mutex);
            m_CV.wait_for(lock, std::chrono::microseconds(200), [this] { return m_Stop.load(std::memory_order_relaxed); });
        }

        t_Current = nullptr;
    }
}
