#pragma once

#include "Base/Base.h"
#include "Memory/Allocation.h"

namespace tyr
{
    // C is capacity
    template<typename T, uint C>
    class FixedArray final
    {
    public:
        FixedArray() = default;

        FixedArray(std::initializer_list<T> list) 
        {
            TYR_ASSERT(list.size() == C);
            std::copy(list.begin(), list.end(), m_Data);
        }

        T& operator[](uint index)
        {
            TYR_ASSERT(index < C);
            return m_Data[index];
        }

        const T& operator[](uint index) const
        {
            TYR_ASSERT(index < C);
            return m_Data[index];
        }

        constexpr uint Capacity() const { return C; }

        T* Data() { return m_Data; }

        const T* Data() const { return m_Data; }

    private:
        T m_Data[C];
    };
}

