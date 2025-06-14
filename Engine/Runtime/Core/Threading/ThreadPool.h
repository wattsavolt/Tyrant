#pragma once

#include "Base/Base.h"
#include "Containers/Containers.h"
#include "Threading.h"
#include "Utility/INonCopyable.h"

namespace tyr
{
    class ThreadTask 
    {
    public:
        ThreadTask() = default;;
        virtual ~ThreadTask() = default;

    protected:
        friend class PooledThread;
        virtual void Run() = 0;
    };

    // Configuration struct for thread pool
    struct ThreadPoolConfig
    {
        uint threadCount = static_cast<uint>(Thread::hardware_concurrency());
    };

    class PooledThread final : public INonCopyable
    {
    public:
        PooledThread();
        ~PooledThread();

        void Start(ThreadTask* task);
        void Wait();
        bool IsAvailable();

    private:
        void Run();

        Thread m_Thread;
        Mutex m_Mutex;
        ConditionVariable m_CV;

        ThreadTask* m_Task;
        std::atomic<bool> m_Stop = false;
        bool m_IsRunningTask = false;
    };

    class ThreadPool final
    {
    public:
        ThreadPool(const ThreadPoolConfig& config);
        ~ThreadPool();

        PooledThread* GetAvailableThread();

    private:
        LocalArray<PooledThread, 64> m_Threads;
        Mutex m_Mutex;
    };

}