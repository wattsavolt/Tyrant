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
    //  - AutoDelete (the default): freed immediately once the task and any subtasks finish.
    //    Every dependent must be registered before this task is enqueued, since it can
    //    vanish at any moment once running.
    //  - ManualRelease: stays valid - safe to add dependents even after it starts or
    //    finishes - until explicitly released. The creator must release it exactly once,
    //    any time after it knows no more dependents will be added.
    enum class TaskLifetime : uint8
    {
        AutoDelete,
        ManualRelease
    };

    // Plain data - the operations that act on it are kept separate.
    struct Task
    {
        TaskFunction function;

        // Starts at 1 ("not yet enqueued"); +1 per dependency added, -1 per dependency
        // finished and -1 on enqueue. Whichever decrement reaches 0 marks the task runnable,
        // so two completions can't each think they're the one to start it.
        Atomic<uint> dependencyCount;
        Atomic<TaskState> state;

        // Grows to this task's largest-ever dependent count, then never reallocates - the
        // same Task object is reset and reused for every task occupying this pool slot.
        // Only safe to read once this task is Finished.
        Array<TaskID> dependents;

        // Starts at 1, for "the task's own function hasn't returned yet". +1 per subtask
        // spawned while running; -1 when the function returns and -1 per spawned subtask
        // that finishes. Whichever of those brings it to 0 is what finishes the task.
        Atomic<int> pendingCount;

        TaskID parent = c_InvalidTaskID;

        TaskLifetime lifetime = TaskLifetime::AutoDelete;

        // Starts at 1 for AutoDelete, 2 for ManualRelease. Reaching 0 is what actually
        // deletes the task's pool slot.
        Atomic<int> deletionGate;

        // Guards dependents together with the Finished state transition, so a dependent
        // registering itself and this task finishing can never race silently. Setup-time
        // only, never touched on the hot run/steal path, so a plain mutex costs nothing there.
        Mutex mutex;

        // Count of tasks that haven't finished running yet, independent of pool-slot
        // lifetime - a ManualRelease task's slot can stay allocated long after it finishes,
        // so tracking completion separately avoids waiting on a release that may never come.
        static Atomic<uint> s_LiveCount;

        Task();

        // Clears a reused task slot back to default values, function included - the object
        // itself is never reconstructed, just reset in place each time a slot is reused.
        void Reset();
    };
}
