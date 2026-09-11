#pragma once

#include "Base/Base.h"

namespace tyr
{
    template<typename>
    class FunctionRef;

    // Use when the function does not need to be stored. Do not store as a member!
    template<typename Ret, typename... Args>
    class FunctionRef<Ret(Args...)>
    {
    private:
        void* m_Object = nullptr;
        Ret(*m_Execute)(void*, Args...) = nullptr;

    public:
        FunctionRef() = default;

        template<typename Lambda>
        FunctionRef(Lambda&& l)
        {
            using LambdaType = std::remove_cvref_t<Lambda>;

            m_Object = (void*)std::addressof(l);

            m_Execute = [](void* obj, Args... args) -> Ret
                {
                    return (*reinterpret_cast<LambdaType*>(obj))(std::forward<Args>(args)...);
                };
        }

        Ret Invoke(Args... args) const
        {
            return m_Execute(m_Object, std::forward<Args>(args)...);
        }

        Ret operator()(Args... args) const
        {
            return Invoke(std::forward<Args>(args)...);
        }

        explicit operator bool() const
        {
            return m_Execute != nullptr;
        }
    };
}
