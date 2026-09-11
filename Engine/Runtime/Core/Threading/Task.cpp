#pragma once

#include "Task.h"

namespace tyr
{
    Task::Task(TaskFunction&& fn)
        : m_Function(std::move(fn))
    {

    }

    Task::Task()
        : m_State(TaskState::Inactive)
    {

    }

    void Task::Run()
    {
        TYR_ASSERT(m_Function && !IsActive());
       
        m_State.store(TaskState::Running, std::memory_order_release);

        m_Function.Invoke();

        m_State.store(TaskState::Finished, std::memory_order_release);
    }

    void Task::SetFunction(TaskFunction&& callable)
    {
        m_Function = std::move(callable);
    }
}