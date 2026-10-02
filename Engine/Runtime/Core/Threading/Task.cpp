#include "Task.h"

namespace tyr
{
    Atomic<uint> Task::s_LiveCount{ 0 };

    Task::Task()
        : dependencyCount(1)
        , state(TaskState::Inactive)
        , pendingCount(1)
        , parent(c_InvalidTaskID)
        , lifetime(TaskLifetime::AutoDelete)
        , deletionGate(1)
    {

    }

    void Task::Reset()
    {
        function = TaskFunction();
        dependencyCount.store(1, std::memory_order_relaxed);
        dependents.Clear();
        pendingCount.store(1, std::memory_order_relaxed);
        parent = c_InvalidTaskID;
        lifetime = TaskLifetime::AutoDelete;
        deletionGate.store(1, std::memory_order_relaxed);
        state.store(TaskState::Inactive, std::memory_order_relaxed);
    }
}
