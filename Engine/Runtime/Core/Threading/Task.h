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

    class Task final
    {
    public:
        static constexpr uint16 c_MaxDependents = 8;

        Task(TaskFunction&& fn);

        Task();

        void SetFunction(TaskFunction&& fn);

        bool IsActive() const
        {
            return m_State.load(std::memory_order_acquire) != TaskState::Inactive;
        }
        
    private:
        friend class PooledThread;
        void Run();

        TaskFunction m_Function;
        Atomic<uint> m_DependencyCount;
        Atomic<TaskState> m_State;
        LocalArray<TaskID, c_MaxDependents> m_Dependents;
    };
}