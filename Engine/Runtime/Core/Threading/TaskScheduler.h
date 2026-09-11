#pragma once

#include "Base/Base.h"
#include "Function/Function.h"
#include "Task.h"

namespace tyr
{
    class ThreadPool;
    class TaskPool;
    class TYR_CORE_API TaskScheduler final : public INonCopyable
    {
    public:
        static constexpr uint c_PermanentThreadCount = 2;
        static constexpr uint c_MaxTasks = 256;
        static const uint c_MaxWorkers;

        TaskScheduler();
        ~TaskScheduler();

        TaskID CreateTask(TaskFunction&& fn);

        void AddDependency(TaskID task, TaskID dependency);

        void Enqueue(TaskID task);

        TaskID CreateAndEnqueueTask(TaskFunction&& fn);

    private:
        ThreadPool* m_ThreadPool;
        TaskPool* m_TaskPool;
    };
}