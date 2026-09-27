#pragma once

#include "Base/Base.h"
#include "ThreadTypes.h"
#include "Containers/WorkStealingDeck.h"
#include "TaskID.h"

namespace tyr
{
    struct WorkerContext
    {
        static constexpr uint c_WorkerQueueCapacity = 1024;
        uint index = 0;
        WorkStealingDeque<TaskID, c_WorkerQueueCapacity> queue;
    };

    class ThreadPool;
    class TaskScheduler;
    struct Task;

    class PooledThread final
    {
    public:
        PooledThread();
        ~PooledThread();

        PooledThread(const PooledThread&) = delete;
        PooledThread& operator=(const PooledThread&) = delete;

        // Gives this thread its place in the pool and starts its worker loop. Must be
        // called exactly once, before any thread in the pool can be stealing from or
        // submitting work to this one - ThreadPool does this right after construction,
        // before any thread's Run() can observe another thread's state.
        void Launch(uint index, ThreadPool* threadPool, TaskScheduler* scheduler);

        // Tells the worker loop to wind down once it's drained all reachable work - see
        // Run(). Doesn't block.
        void RequestStop();

        // Blocks until this worker's OS thread has actually exited. Only safe to call after
        // RequestStop().
        void Join();

        WorkerContext& GetContext() { return m_Context; }

        // The task this thread is currently running, or c_InvalidTaskID if it isn't
        // running one right now. Used to attribute newly-created tasks to their parent.
        TaskID GetCurrentTaskID() const { return m_CurrentTaskId; }

        // The PooledThread executing on the calling thread, or nullptr if the calling
        // thread isn't one of the pool's workers (e.g. it's the main thread).
        static PooledThread* GetCurrent();

    private:
        void Run();
        bool TryGetNextTask(TaskID& outId);

        WorkerContext m_Context;
        ThreadPool* m_ThreadPool = nullptr;
        TaskScheduler* m_Scheduler = nullptr;
        TaskID m_CurrentTaskId = c_InvalidTaskID;

        Thread m_Thread;
        Mutex m_Mutex;
        ConditionVariable m_CV;
        Atomic<bool> m_Stop = false;

        thread_local static PooledThread* t_Current;
    };
}
