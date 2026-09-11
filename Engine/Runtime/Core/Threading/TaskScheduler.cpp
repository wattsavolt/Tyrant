#pragma once

#include "TaskScheduler.h"
#include "ThreadPool.h"
#include "TaskPool.h"

namespace tyr
{
    const uint TaskScheduler::c_MaxWorkers = TYR_MAX_CONCURRENT_THREADS - c_PermanentThreadCount;

    TaskScheduler::TaskScheduler()
        : m_TaskPool(new TaskPool())
    {
        {
            ThreadPoolConfig config;
            config.threadCount = c_MaxWorkers;
            m_ThreadPool = new ThreadPool(config);
        }
    }

    TaskScheduler::~TaskScheduler()
    {
        delete m_TaskPool;
        delete m_ThreadPool;
    }

    TaskID TaskScheduler::CreateTask(TaskFunction&& fn)
    {
        TaskID id = m_TaskPool->Create();
        return id;
    }

    void TaskScheduler::AddDependency(TaskID task, TaskID dependency)
    {

    }

    void TaskScheduler::Enqueue(TaskID task)
    {

    }

    TaskID TaskScheduler::CreateAndEnqueueTask(TaskFunction&& fn)
    {
        TaskID id = m_TaskPool->Create();
        return id;
    }
}