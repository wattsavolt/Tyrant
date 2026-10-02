#include "TaskUtil.h"

namespace tyr
{
    void TaskUtil::InitTask(Task& task, TaskFunction&& fn, TaskLifetime lifetime)
    {
        task.Reset();
        task.function = std::move(fn);
        task.lifetime = lifetime;
        task.deletionGate.store(lifetime == TaskLifetime::ManualRelease ? 2 : 1, std::memory_order_relaxed);

        // Counts this task as live until it actually finishes running, released the instant
        // its state becomes Finished - independent of whenever its slot is eventually freed.
        Task::s_LiveCount.fetch_add(1, std::memory_order_relaxed);
    }

    bool TaskUtil::IsActive(const Task& task)
    {
        return task.state.load(std::memory_order_acquire) != TaskState::Inactive;
    }

    bool TaskUtil::IsFinished(const Task& task)
    {
        return task.state.load(std::memory_order_acquire) == TaskState::Finished;
    }

    bool TaskUtil::Run(Task& task)
    {
        TYR_ASSERT(task.function && GetState(task) == TaskState::Pending);

        SetState(task, TaskState::Running);

        task.function.Invoke();

        return ReleasePendingRef(task);
    }

    void TaskUtil::AddPendingRef(Task& task)
    {
        task.pendingCount.fetch_add(1, std::memory_order_relaxed);
    }

    bool TaskUtil::ReleasePendingRef(Task& task)
    {
        const int remaining = task.pendingCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
        TYR_ASSERT(remaining >= 0);

        if (remaining != 0)
        {
            return false;
        }

        // Setting Finished has to happen under the same lock dependent-registration checks
        // state under - otherwise a dependent could register itself right as this runs and
        // never get released, or this could finish believing it has none when one was about to be added.
        {
            LockGuard guard(task.mutex);
            SetState(task, TaskState::Finished);
        }

        // This task is genuinely done now, regardless of whether its pool slot is ever
        // actually released.
        Task::s_LiveCount.fetch_sub(1, std::memory_order_relaxed);

        return true;
    }

    void TaskUtil::AddDependencyRef(Task& task)
    {
        task.dependencyCount.fetch_add(1, std::memory_order_relaxed);
    }

    bool TaskUtil::ReleaseDependencyRef(Task& task)
    {
        return task.dependencyCount.fetch_sub(1, std::memory_order_acq_rel) == 1;
    }

    bool TaskUtil::TryAddDependent(Task& task, TaskID dependent)
    {
        LockGuard guard(task.mutex);

        if (GetState(task) == TaskState::Finished)
        {
            return false;
        }

        task.dependents.Add(dependent);
        return true;
    }

    void TaskUtil::SetState(Task& task, TaskState state)
    {
        task.state.store(state, std::memory_order_release);
    }

    TaskState TaskUtil::GetState(const Task& task)
    {
        return task.state.load(std::memory_order_acquire);
    }

    bool TaskUtil::ReleaseDeletionGate(Task& task)
    {
        const int remaining = task.deletionGate.fetch_sub(1, std::memory_order_acq_rel) - 1;
        TYR_ASSERT(remaining >= 0);
        return remaining == 0;
    }
}
