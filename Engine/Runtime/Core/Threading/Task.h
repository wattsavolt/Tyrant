#pragma once

#include "Base/Base.h"
#include "Function/Function.h"
#include "Containers/Containers.h"
#include "Memory/PoolHandle.h"
#include "Threading/ThreadTypes.h"
#include "TaskID.h"

namespace tyr
{
    enum class TaskState : uint8
    {
        Inactive,
        Pending,
        Running,
        Finished
    };

    using TaskFunction = Function<void()>;

    // Controls when a finished task's pool slot is actually freed.
    //
    //  - AutoDelete (the default): freed the instant the task (and any subtasks it spawned)
    //    finish. Zero extra cost, but every dependent must be registered via AddDependency
    //    *before* this task is enqueued - once it's running it may finish and be deleted at
    //    any moment, so adding a dependency on it after that point isn't safe. This covers
    //    the common case where a dependency graph is built up front, all at once, before any
    //    of its nodes start running (e.g. render graph passes wiring up their dependencies
    //    on each other before any of them are kicked off).
    //
    //  - ManualRelease: stays valid - safe to AddDependency against, even after it's started
    //    or already finished running - until TaskScheduler::ReleaseTask() is explicitly
    //    called on it. Use this when a task might need to gain a new dependent sometime
    //    after it could already be running or done, which AutoDelete can't safely support
    //    (e.g. next frame's render-submission task depending on this frame's, set up from
    //    the main thread on a later tick, well after this frame's task may have finished).
    //    The creator must call ReleaseTask exactly once, whenever it knows no more
    //    dependencies will ever be added - that can happen before or after the task itself
    //    finishes running; the slot is only freed once both have happened. Once released,
    //    treat the ID as gone, exactly like an AutoDelete task - don't add further
    //    dependencies to it after that point.
    enum class TaskLifetime : uint8
    {
        AutoDelete,
        ManualRelease
    };

    // Plain data - see TaskUtil for everything that operates on a Task. Reset() is kept here
    // since it's the struct's own "clear back to a blank state" operation, same shape as
    // Reset() on other plain data structs elsewhere (MeshHeader, ModelImportMesh, etc.) -
    // TaskUtil::InitTask is what actually prepares a task for a new job (calls Reset, then
    // sets it up with a function/lifetime).
    struct Task
    {
        TaskFunction function;

        // Starts at 1, for "this task hasn't been Enqueue()'d yet". +1 per real dependency
        // added via AddDependency(); Enqueue() itself releases the initial ref, same as every
        // dependency finishing releases its own. Whichever of those - the last dependency
        // finishing, or Enqueue() being called - brings it to 0 is what actually makes the
        // task runnable. This mirrors pendingCount's own bias below, and for the same reason:
        // without it, Enqueue() and a dependency's completion could each independently decide
        // (from GetDependencyCount()==0 and a separate state check) that they're the one that
        // should push the task, racing to push it twice.
        Atomic<uint> dependencyCount;
        Atomic<TaskState> state;

        // Grows to whatever a task's largest-ever dependent count has been, then never
        // reallocates again, since this same Task object is reset and reused for every
        // task that ever occupies this pool slot (see TaskPool) rather than being freed.
        // Only safe to read directly once this task is Finished - see
        // TaskUtil::TryAddDependent/ReleasePendingRef.
        Array<TaskID> dependents;

        // Starts at 1, for "the task's own function hasn't returned yet". +1 per subtask
        // spawned while running; -1 when the function returns and -1 per spawned subtask
        // that finishes. Whichever of those brings it to 0 is what finishes the task.
        Atomic<int> pendingCount;

        TaskID parent = c_InvalidTaskID;

        TaskLifetime lifetime = TaskLifetime::AutoDelete;

        // Starts at 1 for AutoDelete, 2 for ManualRelease - see TaskLifetime above and
        // TaskUtil::ReleaseDeletionGate. Reaching 0 is what actually deletes the task's pool
        // slot.
        Atomic<int> deletionGate;

        // Guards dependents together with the Finished state transition, so a dependent
        // registering itself and this task finishing can never race each other silently -
        // see TaskUtil::TryAddDependent/ReleasePendingRef. Setup-time only, never touched on
        // the per-task steal/run hot path, so a plain mutex here doesn't cost anything there.
        Mutex mutex;

        Task();

        // Puts a task slot pulled back out of the pool into a clean, blank state - the
        // struct's own default values, function included. Must be called every time a slot
        // is reused (see TaskPool) - the Task object itself isn't reconstructed on reuse.
        // Doesn't take a function/lifetime itself - see TaskUtil::InitTask for actually
        // preparing a reset task for a new job.
        void Reset();
    };
}
