#pragma once

#include "ThreadPool.h"
#include "Task.h"

namespace tyr
{
    PooledThread::PooledThread()
    {
        m_Thread = Thread(&PooledThread::Run, this);
    }

    PooledThread::~PooledThread()
    {
        {
            LockGuard guard(m_Mutex);
            m_Stop = true;
        }
        m_CV.notify_one();
        if (m_Thread.joinable())
        {
            m_Thread.join();
        }
    }

    void PooledThread::Run()
    {
        while (true)
        {
            Task* task = nullptr;
            {
                Lock lock(m_Mutex);
                // Wait unlocks the mutex while waiting
                m_CV.wait(lock, [this]() { return m_Task || m_Stop; });

                if (m_Stop)
                {
                    m_Task = nullptr;
                    break;
                }

                task = m_Task;
                m_Task = nullptr;
            }

            // Run the task outside the lock
            task->Run();

            {
                LockGuard guard(m_Mutex);
                m_IsRunningTask = false;
            }

            m_CV.notify_one();
        }
    }

    void PooledThread::Start(Task* task)
    {
        {
            Lock lock(m_Mutex);
            // Wait unlocks the mutex while waiting
            m_CV.wait(lock, [this] { return !m_IsRunningTask; });
            m_Task = task;
            m_IsRunningTask = true;
        }
        m_CV.notify_one();
    }

    void PooledThread::Wait()
    {
        Lock lock(m_Mutex);
        m_CV.wait(lock, [this] { return !m_IsRunningTask; });
    }

    bool PooledThread::IsAvailable()
    {
        LockGuard guard(m_Mutex);
        return !m_IsRunningTask;
    }

    ThreadPool::ThreadPool(const ThreadPoolConfig& config)
    {
        m_Threads.Resize(config.threadCount);
    }

    ThreadPool::~ThreadPool()
    {
        
    }

    PooledThread* ThreadPool::GetAvailableThread()
    {
        LockGuard guard(m_Mutex);
        for (PooledThread& thread : m_Threads)
        {
            if (thread.IsAvailable())
            {
                return &thread;
            }
        }
        return nullptr;
    }
}