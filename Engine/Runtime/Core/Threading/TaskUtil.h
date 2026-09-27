#pragma once

#include "Task.h"

namespace tyr
{
    // Everything that operates on a Task - see Task.h for why the data and the functions
    // that work on it are kept separate.
    class TaskUtil final
    {
    public:
        // Prepares 'task' for a new job: clears it back to a blank state (Task::Reset) and
        // then sets its function and lifetime. Must be called every time a pool slot is
        // reused (see TaskPool) - the Task object itself isn't reconstructed on reuse.
        static void InitTask(Task& task, TaskFunction&& fn, TaskLifetime lifetime = TaskLifetime::AutoDelete);

        static bool IsActive(const Task& task);
        static bool IsFinished(const Task& task);

        // Runs this task's function. Returns true if the task is now fully finished - i.e.
        // it spawned no subtasks while running, or all of them had already finished by the
        // time this returned. Returns false if it must wait on outstanding subtasks;
        // whichever of those finishes last is what actually finishes this task.
        static bool Run(Task& task);

        // One more reason this task isn't finished yet: either "its function hasn't
        // returned" (there's always exactly one of these, added at InitTask time) or "a
        // subtask it spawned hasn't finished". Releasing the last one is what finishes it.
        static void AddPendingRef(Task& task);
        static bool ReleasePendingRef(Task& task); // returns true if this was the last one (task is finished)

        // Also called once by TaskScheduler::Enqueue() itself, releasing the initial "not
        // enqueued yet" ref - see Task::dependencyCount's comment for why.
        static void AddDependencyRef(Task& task);
        static bool ReleaseDependencyRef(Task& task); // returns true if this was the last one (task is runnable)

        // Registers 'dependent' to be released once 'task' finishes. Returns false if 'task'
        // has already finished by the time this is called - there's nothing to wait for in
        // that case, and the caller must not treat the dependency as pending (it will never
        // be released, since task's finish processing has already run). Safe to call
        // concurrently for the same task from multiple threads. Task::dependents itself is
        // only safe to read directly once the task is Finished - see its own comment.
        static bool TryAddDependent(Task& task, TaskID dependent);

        // Task::state is atomic and these use specific memory orders (weaker than the
        // implicit default of a plain load/store on it), so they stay as wrappers rather
        // than having every call site pick an ordering itself.
        static void SetState(Task& task, TaskState state);
        static TaskState GetState(const Task& task);

        // One fewer reason this task's slot can't be freed yet: either "it hasn't finished
        // running" or, for a ManualRelease task, "its creator hasn't released it". Returns
        // true if this was the last one - whichever of those two events happens last is the
        // one that actually deletes the task, so this can safely be called from either side
        // without the two racing each other (same fetch_sub-to-zero pattern as
        // ReleasePendingRef/ReleaseDependencyRef above, no locking needed).
        static bool ReleaseDeletionGate(Task& task);
    };
}
