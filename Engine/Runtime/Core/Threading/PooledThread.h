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
        uint index;
        WorkStealingDeque<TaskID, c_WorkerQueueCapacity> queue;
    };

    struct Task;
    class PooledThread final
    {
    public:
        PooledThread();
        ~PooledThread();

        PooledThread(const PooledThread&) = delete;
        PooledThread& operator=(const PooledThread&) = delete;

        void Start(Task* task);
        void Wait();
        bool IsAvailable();

    private:
        void Run();

        Thread m_Thread;
        Mutex m_Mutex;
        ConditionVariable m_CV;

        Task* m_Task{};
        Atomic<bool> m_Stop = false;
        bool m_IsRunningTask = false;
    };
}