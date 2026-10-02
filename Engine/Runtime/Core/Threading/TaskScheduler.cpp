#include "TaskScheduler.h"
#include "ThreadPool.h"
#include "TaskPool.h"
#include "PooledThread.h"
#include "TaskUtil.h"

namespace tyr
{
    const uint TaskScheduler::c_MaxWorkers = TYR_MAX_CONCURRENT_THREADS - c_PermanentThreadCount;

    TaskScheduler& TaskScheduler::Instance()
    {
        static TaskScheduler scheduler;
        return scheduler;
    }

    TaskScheduler::TaskScheduler()
        : m_TaskPool(new TaskPool())
    {
        ThreadPoolConfig config;
        config.threadCount = c_MaxWorkers;
        m_ThreadPool = new ThreadPool(config, this);
    }

    TaskScheduler::~TaskScheduler()
    {
        delete m_ThreadPool;
        delete m_TaskPool;
    }

    TaskID TaskScheduler::CreateTask(TaskFunction&& fn, TaskLifetime lifetime)
    {
        const TaskID id = m_TaskPool->Create();

        Task& task = (*m_TaskPool)[id];
        TaskUtil::InitTask(task, std::move(fn), lifetime);

        if (PooledThread* current = PooledThread::GetCurrent())
        {
            const TaskID parentId = current->GetCurrentTaskID();
            if (parentId != c_InvalidTaskID)
            {
                task.parent = parentId;
                TaskUtil::AddPendingRef((*m_TaskPool)[parentId]);
            }
        }

        return id;
    }

    void TaskScheduler::AddDependency(TaskID task, TaskID dependency)
    {
        Task& t = (*m_TaskPool)[task];
        Task& d = (*m_TaskPool)[dependency];

        // Dependencies must be fully set up before the task is enqueued - once it's
        // enqueued it may already be running (or finished) on another thread.
        TYR_ASSERT(TaskUtil::GetState(t) == TaskState::Inactive);

        // Only safe to reference 'dependency' here if it can't have been deleted already:
        // either it hasn't been enqueued yet, or it was created as ManualRelease (safe at
        // any time until its creator releases it).
        TYR_ASSERT(TaskUtil::GetState(d) == TaskState::Inactive || d.lifetime == TaskLifetime::ManualRelease);

        // Only counted as a real dependency if we won the race to register before
        // 'dependency' finished - if it had already finished, there's nothing to wait for.
        if (TaskUtil::TryAddDependent(d, task))
        {
            TaskUtil::AddDependencyRef(t);
        }
    }

    void TaskScheduler::Enqueue(TaskID task)
    {
        Task& t = (*m_TaskPool)[task];

        TaskUtil::SetState(t, TaskState::Pending);

        // Releases the initial "not enqueued yet" ref - only pushes if this is the last
        // outstanding ref, i.e. every dependency had already finished before this call. A
        // single atomic decrement-to-zero is what stops two completions racing to push it twice.
        if (TaskUtil::ReleaseDependencyRef(t))
        {
            PushRunnable(task);
        }
        // Otherwise it stays Pending, unqueued, until its last outstanding dependency finishes.
    }

    TaskID TaskScheduler::CreateAndEnqueueTask(TaskFunction&& fn, TaskLifetime lifetime)
    {
        const TaskID id = CreateTask(std::move(fn), lifetime);
        Enqueue(id);
        return id;
    }

    bool TaskScheduler::IsTaskFinished(TaskID task) const
    {
        return TaskUtil::IsFinished((*m_TaskPool)[task]);
    }

    void TaskScheduler::WaitOnTask(TaskID task) const
    {
        while (!IsTaskFinished(task))
        {
            TYR_THREAD_SLEEP_MS(0);
        }
    }

    void TaskScheduler::ReleaseTask(TaskID task)
    {
        Task& t = (*m_TaskPool)[task];

        TYR_ASSERT(t.lifetime == TaskLifetime::ManualRelease);

        if (TaskUtil::ReleaseDeletionGate(t))
        {
            m_TaskPool->Delete(task);
        }
    }

    void TaskScheduler::FlushCurrentThreadCache()
    {
        m_TaskPool->FlushCurrentThreadCache();
    }

    void TaskScheduler::WaitForAllTasks() const
    {
        while (Task::s_LiveCount.load(std::memory_order_acquire) != 0)
        {
            TYR_THREAD_SLEEP_MS(0);
        }
    }

    void TaskScheduler::PushRunnable(TaskID task)
    {
        if (PooledThread* current = PooledThread::GetCurrent())
        {
            if (current->GetContext().queue.Push(task))
            {
                return;
            }
            // This worker's own queue is full (1024 deep) - fall back to the injector
            // rather than silently dropping the task.
        }

        const bool enqueued = m_InjectorQueue.Enqueue(task);
        // 256 deep and meant for occasional external submission, not a firehose - hitting
        // this means a real backlog, not routine usage.
        TYR_ASSERT(enqueued);
        (void)enqueued; // avoid an unused-variable warning in release, where the assert compiles out
    }

    bool TaskScheduler::TryPopInjector(TaskID& outId)
    {
        if (Optional<TaskID> id = m_InjectorQueue.Dequeue())
        {
            outId = *id;
            return true;
        }

        return false;
    }

    void TaskScheduler::OnTaskFinished(TaskID task)
    {
        Task& t = (*m_TaskPool)[task];

        for (TaskID dependentId : t.dependents)
        {
            Task& dependent = (*m_TaskPool)[dependentId];
            if (TaskUtil::ReleaseDependencyRef(dependent))
            {
                PushRunnable(dependentId);
            }
        }

        const TaskID parentId = t.parent;
        if (parentId != c_InvalidTaskID)
        {
            Task& parent = (*m_TaskPool)[parentId];
            if (TaskUtil::ReleasePendingRef(parent))
            {
                OnTaskFinished(parentId);
            }
        }

        // Last, since it may free t's slot: for an AutoDelete task this is the only thing
        // guarding deletion, so it fires immediately; for ManualRelease it only deletes once
        // ReleaseTask has also been called, whichever of the two happens last.
        if (TaskUtil::ReleaseDeletionGate(t))
        {
            m_TaskPool->Delete(task);
        }
    }
}
