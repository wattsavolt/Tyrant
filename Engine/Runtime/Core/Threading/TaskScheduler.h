#pragma once

#include "Base/Base.h"
#include "Function/Function.h"
#include "Containers/Containers.h"
#include "ThreadTypes.h"
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

        static TaskScheduler& Instance();

        // Creates a task and sets its function but doesn't schedule it - call Enqueue() once
        // ready. "lifetime" controls slot-freeing - leave at AutoDelete unless a dependency
        // might be added later, after it could already be running or finished.
        TaskID CreateTask(TaskFunction&& fn, TaskLifetime lifetime = TaskLifetime::AutoDelete);

        // Makes "task" wait for "dependency" to finish before it can run - must be called
        // before task is enqueued. "dependency" is only safe to reference if it's
        // ManualRelease, or hasn't been enqueued itself yet.
        void AddDependency(TaskID task, TaskID dependency);

        // Marks a task ready to run. If it has outstanding dependencies it waits for them;
        // otherwise it's pushed onto a queue immediately. Safe to call from any thread.
        void Enqueue(TaskID task);

        TaskID CreateAndEnqueueTask(TaskFunction&& fn, TaskLifetime lifetime = TaskLifetime::AutoDelete);

        // Only reliable on a task known not to have finished and been auto-deleted yet - e.g.
        // one created with ManualRelease, or one known still in flight.
        bool IsTaskFinished(TaskID task) const;

        // Spin-waits (yielding each iteration) until the task is finished - only safe to call
        // on a task known not to have already finished and been auto-deleted.
        void WaitOnTask(TaskID task) const;

        // Tells the scheduler a ManualRelease task's creator won't add any more dependencies
        // to it - must be called exactly once per such task. Its slot frees once this has
        // happened and the task itself finished, in either order. Not valid on an AutoDelete task.
        void ReleaseTask(TaskID task);

        // Returns any task slots the calling thread has freed back to the shared pool. Worker
        // threads do this automatically while idle; any other task-creating/deleting thread
        // should call this periodically itself.
        void FlushCurrentThreadCache();

        // Spin-waits until every task ever created has finished running - not until the task
        // pool is empty, since a ManualRelease task's slot can stay allocated after it
        // finishes. Meant to be called once, before any shutdown code runs, from the main thread.
        void WaitForAllTasks() const;

    private:
        friend class PooledThread;

        TaskScheduler();
        ~TaskScheduler();

        TaskPool& GetTaskPool() { return *m_TaskPool; }

        // Pushes a task onto whichever queue is correct for the calling thread: its own
        // worker queue if it's running as a task, or the shared injector queue otherwise.
        void PushRunnable(TaskID task);

        bool TryPopInjector(TaskID& outId);

        // Called once a task (or a child of it) has fully finished. Unblocks any dependents
        // whose last outstanding dependency this was, and propagates completion up to its
        // parent if it has one.
        void OnTaskFinished(TaskID task);

        ThreadPool* m_ThreadPool;
        TaskPool* m_TaskPool;

        // Where a non-worker thread's task submissions land, since it has no worker queue of
        // its own. Lock-free and fixed-depth - external submission is expected to be
        // occasional, not a firehose.
        static constexpr uint c_InjectorCapacity = 256;
        MPMCRingBuffer<TaskID, c_InjectorCapacity> m_InjectorQueue;
    };
}
