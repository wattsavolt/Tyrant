#pragma once

#include "Base/Base.h"

namespace tyr
{
    template<typename>
    class Function;

    // Must be moved. Copy not supported
    template<typename Ret, typename... Args>
    class Function<Ret(Args...)>
    {
    private:
        static constexpr size_t c_BufferSize = 32; // enough for 4 pointer / reference captures
        alignas(void*) char m_Buffer[c_BufferSize];

        Ret(*m_Execute)(void*, Args&&...) = nullptr;

    public:
        Function() = default;

        template<typename Lambda>
        Function(Lambda&& l)
        {
            using Decayed = std::decay_t<Lambda>;

            TYR_STATIC_ASSERT(sizeof(Decayed) <= c_BufferSize,"Lambda too big; exceeds max captures");
            // Moving/destroying this Function just copies its raw bytes around (see the move
            // constructor below) rather than calling the captured lambda's own move/destructor.
            // That's only correct if the lambda is trivially copyable, e.g. it only captures
            // pointers, references or plain data - never something like a String or Array that
            // owns memory and needs real move/copy logic to run.
            TYR_STATIC_ASSERT(std::is_trivially_copyable_v<Decayed>, "Captured lambda must be trivially copyable - capture pointers/references/POD data only");

            new (m_Buffer) Decayed(std::forward<Lambda>(l));

            m_Execute = [](void* buf, Args&&... args) -> Ret
                {
                    return (*reinterpret_cast<Decayed*>(buf))(std::forward<Args>(args)...);
                };
        }

        Function(const Function&) = delete;
        Function& operator=(const Function&) = delete;

        Function(Function&& other) noexcept
        {
            m_Execute = other.m_Execute;
            memcpy(m_Buffer, other.m_Buffer, c_BufferSize);
            other.m_Execute = nullptr;
        }

        Function& operator=(Function&& other) noexcept
        {
            if (this != &other)
            {
                m_Execute = other.m_Execute;
                memcpy(m_Buffer, other.m_Buffer, c_BufferSize);
                other.m_Execute = nullptr;
            }
            return *this;
        }

        Ret Invoke(Args... args) const
        {
            if (m_Execute)
            {
                return m_Execute((void*)m_Buffer,
                    std::forward<Args>(args)...);
            }

            if constexpr (!std::is_void_v<Ret>)
            {
                return Ret();
            }
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