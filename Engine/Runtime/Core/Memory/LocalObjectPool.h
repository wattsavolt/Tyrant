#pragma once

#include "Base/Base.h"
#include "Containers/LocalArray.h"
#include "PoolAssert.h"

namespace tyr
{
    // Local object pool that stores a C-style array of objects.
    template<class T, uint N, bool ReconstructOnFree = true>
    class LocalObjectPool final
    {
    public:
        LocalObjectPool() = default;

        ~LocalObjectPool()
        {
            TYR_ASSERT(m_ObjectCount == 0);
        }

        bool IsValid(Handle h) const
        {
            return h.index < m_Pos && m_Generations[h.index] == h.generation;
        }

        Handle Create()
        {
            TYR_ASSERT(m_ObjectCount < N);

            uint index;
            if (!m_FreeSpaces.IsEmpty())
            {
                index = m_FreeSpaces.Back();
                m_FreeSpaces.PopBack();
            }
            else
            {
                index = m_Pos++;
            }

            m_ObjectCount++;

            return Handle{ index, m_Generations[index] };
        }

        void Delete(Handle h)
        {
            TYR_ASSERT(IsValid(h));

            const uint index = h.index;

            m_Generations[index]++;
            m_FreeSpaces.Add(index);
            m_ObjectCount--;

            if constexpr (ReconstructOnFree)
            {
                m_Pool[index] = {};
            }
        }

        T& operator[](Handle h)
        {
            TYR_POOL_VALIDATION_ASSERT(IsValid(h));
            return m_Pool[h.index];
        }

        const T& operator[](Handle h) const
        {
            TYR_POOL_VALIDATION_ASSERT(IsValid(h));
            return m_Pool[h.index];
        }

        T& operator[](uint index)
        {
            return m_Pool[index];
        }

        const T& operator[](uint index) const
        {
            return m_Pool[index];
        }

        bool IsEmpty() const { return m_ObjectCount == 0; }

    private:
        T m_Pool[N]{};
        uint m_Generations[N]{};
        LocalArray<uint, N> m_FreeSpaces;

        uint m_Pos = 0;
        uint m_ObjectCount = 0;
    };
}

