#pragma once

#include "ThreadPool.h"
#include "Task.h"
#include "PooledThread.h"

namespace tyr
{
    ThreadPool::ThreadPool(const ThreadPoolConfig& config)
        : m_Threads(config.threadCount)
    {

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