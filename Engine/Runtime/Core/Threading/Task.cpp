#pragma once

#include "Task.h"

namespace tyr
{
    Task::Task(Callable&& callable)
        : m_Callable(std::move(callable))
    {

    }

    // CallableUtil::Execute( will assert if the callable's Execute function is nullptr
    Task::Task()
        : m_State(TaskState::Inactive)
    {

    }

    void Task::Run()
    {
        TYR_ASSERT(m_Callable && !IsActive());
       
        m_State.store(TaskState::Running, std::memory_order_release);

        m_Callable.Invoke();

        m_State.store(TaskState::Finished, std::memory_order_release);
    }

    void Task::SetCallable(Callable&& callable)
    {
        m_Callable = std::move(callable);
    }
}