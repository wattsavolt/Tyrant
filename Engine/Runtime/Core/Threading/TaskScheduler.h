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

        // Creates a task and sets its function, but doesn't schedule it for execution -
        // call Enqueue() once it's ready to run (e.g. once AddDependency() calls for it
        // are done). Safe to call from any thread, including from inside a running task -
        // doing so makes the new task a child of the currently running one, so the parent
        // won't be considered finished until this task (and any others it spawns) finish.
        //
        // "lifetime" controls when the task's slot is freed - see TaskLifetime in Task.h.
        // Leave it at the default (AutoDelete) unless something might need to add a
        // dependency on this task *after* it could already be running or finished (e.g. a
        // recurring task depending on last frame's instance of itself, wired up from a
        // later tick) - use ManualRelease for that, and call ReleaseTask() on it yourself
        // once you know no more dependencies will ever be added to it.
        TaskID CreateTask(TaskFunction&& fn, TaskLifetime lifetime = TaskLifetime::AutoDelete);

        // Makes "task" wait for "dependency" to finish before it can run.
        //
        // Must be called before Enqueue(task) - "task" itself can't already be running.
        // "dependency", on the other hand, is only safe to reference here if either: it was
        // created with TaskLifetime::ManualRelease (safe at any time, including after it's
        // started or finished, right up until its creator calls ReleaseTask on it), or it
        // hasn't been enqueued yet either - i.e. the whole dependency graph between a group
        // of AutoDelete tasks is being wired up together, before any of them start running
        // (the common case - e.g. render graph passes declaring dependencies on each other
        // before any are kicked off). Adding a dependency on an AutoDelete task that might
        // already be running isn't safe - it could finish and be deleted at any moment.
        void AddDependency(TaskID task, TaskID dependency);

        // Marks a task ready to run. If it has outstanding dependencies it waits for them;
        // otherwise it's pushed onto a queue immediately. Safe to call from any thread.
        void Enqueue(TaskID task);

        TaskID CreateAndEnqueueTask(TaskFunction&& fn, TaskLifetime lifetime = TaskLifetime::AutoDelete);

        // Only reliable to call on a task you know hasn't had a chance to finish and be
        // auto-deleted yet (e.g. one created with TaskLifetime::ManualRelease, or one you're
        // certain is still in flight) - an AutoDelete task may already be gone by the time
        // you ask.
        bool IsTaskFinished(TaskID task) const;

        // Spin-waits (yielding the calling thread each iteration) until IsTaskFinished(task) is
        // true. Same task-liveness caveat as IsTaskFinished - only call this on a task you know
        // is still safe to query.
        void WaitOnTask(TaskID task) const;

        // Tells the scheduler a ManualRelease task's creator won't be adding any more
        // dependencies on it. Must be called exactly once per ManualRelease task, whenever
        // that's known to be true - it can happen before or after the task itself finishes
        // running; its slot is only actually freed once both have happened. Not valid to
        // call on an AutoDelete task (asserts) - there's nothing to release, and it may
        // already be gone.
        void ReleaseTask(TaskID task);

        // Returns any task slots the calling thread has freed back to the shared pool.
        // Worker threads do this automatically while idle; call this periodically from any
        // other thread that creates/deletes tasks (e.g. once per frame on main) - see
        // TaskPool::FlushCurrentThreadCache for why this is needed at all.
        void FlushCurrentThreadCache();

    private:
        friend class PooledThread;

        TaskScheduler();
        ~TaskScheduler();

        TaskPool& GetTaskPool() { return *m_TaskPool; }

        // Pushes a task onto whichever queue is correct for the calling thread: its own
        // worker queue if it's running as a task, or the shared injector queue otherwise.
        void PushRunnable(TaskID task);

        bool TryPopInjector(TaskID& outId);

        // Called once a task (Task::Run() or a child of it) has fully finished. Unblocks
        // any dependents whose last outstanding dependency this was, and propagates
        // completion up to the task's parent if it has one.
        void OnTaskFinished(TaskID task);

        ThreadPool* m_ThreadPool;
        TaskPool* m_TaskPool;

        // Where a non-worker thread's CreateAndEnqueueTask/Enqueue calls land, since it has
        // no worker queue of its own to push onto. Lock-free (multiple non-worker threads
        // can submit concurrently, and any idle worker can dequeue), fixed at a generous
        // 256 deep - external submission is expected to be occasional, not a firehose.
        static constexpr uint c_InjectorCapacity = 256;
        MPMCRingBuffer<TaskID, c_InjectorCapacity> m_InjectorQueue;
    };
}
