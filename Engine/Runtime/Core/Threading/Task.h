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

    class Task final
    {
    public:
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

        std::atomic<TaskState> m_State;
    };
   
}