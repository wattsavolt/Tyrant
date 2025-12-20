#pragma once

#include "Base/Base.h"
#include "Callable.h"
#include "Containers/Containers.h"
#include <memory>

namespace tyr
{
    enum class TaskState : uint8
    {
        Inactive,
        Pending,
        Running,
        Finished
    };

    using TaskID = uint;

    class Task final
    {
    public:
        static constexpr uint16 c_MaxDependents = 8;

        Task(Callable&& callable);

        Task();

        void SetCallable(Callable&& callable);

        bool IsActive() const
        {
            return m_State.load(std::memory_order_acquire) != TaskState::Inactive;
        }
        
    private:
        friend class PooledThread;
        void Run();

        Callable m_Callable;
        Atomic<uint> m_DependencyCount;
        Atomic<TaskState> m_State;
        LocalArray<TaskID, c_MaxDependents> m_Dependents;
    };
   
    class TaskScheduler final
    {
    public:
        TaskScheduler();
        ~TaskScheduler();

        TaskID CreateTask(Callable&& callable); 

        void AddDependency(TaskID task, TaskID dependency);

        void Enqueue(TaskID task);

        TaskID CreateAndEnqueueTask(Callable&& callable);

    private:

    };
}