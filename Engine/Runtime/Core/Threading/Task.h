#pragma once

#include "Base/Base.h"
#include "Containers/Containers.h"
#include "ThreadPool.h"
#include <memory>
#include <functional>

namespace tyr
{
    enum class ThreadTaskState
    {
        Inactive,
        Active
    };

    class Task final : public ThreadTask
    {
    public:
        Task();

        template<typename Func>
        void SetFunction(Func&& func)
        {
            m_Func = std::function<void()>(std::forward<Func>(func));
        }

        template<typename Func>
        void SetFunction(Func& func)
        {
            m_Func = func;
        }

        bool IsActive() const
        {
            return m_State.load(std::memory_order_acquire) == ThreadTaskState::Inactive;
        }

    protected:
        void Run() override;

    private:
        std::function<void()> m_Func;
        Atomic<ThreadTaskState> m_State;
    };
}