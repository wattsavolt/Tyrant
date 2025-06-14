#pragma once

#include "Task.h"

namespace tyr
{
    Task::Task()
        : m_State(ThreadTaskState::Inactive)
    {

    }

    void Task::Run()
    {
        TYR_ASSERT(!IsActive());
        m_State.store(ThreadTaskState::Active, std::memory_order_release);
        m_Func();
        m_State.store(ThreadTaskState::Inactive, std::memory_order_release);
    }
}