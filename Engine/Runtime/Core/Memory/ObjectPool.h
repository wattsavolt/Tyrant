#pragma once

#include "Base/Base.h"
#include "Containers/Array.h"
#include "PoolAssert.h"

namespace tyr
{
    template<class T>
    class ObjectPool final
    {
    public:
        ObjectPool(uint maxObjects)
            : m_MaxObjects(maxObjects)
        {
            m_Pool = MemAllocN<T>(maxObjects);

            m_Generations.Resize(maxObjects);
            memset(m_Generations.Data(), 0, sizeof(uint) * maxObjects);

            m_FreeSpaces.Reserve(m_MaxObjects);
        }

        ~ObjectPool()
        {
            TYR_ASSERT(m_ObjectCount == 0);
            Free(m_Pool);
        }

        bool IsValid(Handle h) const
        {
            return h.index < m_Pos && m_Generations[h.index] == h.generation;
        }

        template<class... Args>
        Handle Create(Args&&... args)
        {
            TYR_ASSERT(m_ObjectCount < m_MaxObjects);

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

            T* object = &m_Pool[index];
            Construct<T>(object, std::forward<Args>(args)...);

            m_ObjectCount++;

            return Handle{ index, m_Generations[index] };
        }

        void Delete(Handle h)
        {
            TYR_ASSERT(IsValid(h));

            const uint index = h.index;

            m_Pool[index].~T();
            m_Generations[index]++;
            m_FreeSpaces.Add(index);

            m_ObjectCount--;
        }

        // Safe handle access
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

        // Fast index access (renderer/internal)
        T& operator[](uint index)
        {
            return m_Pool[index];
        }

        const T& operator[](uint index) const
        {
            return m_Pool[index];
        }

    private:
        T* m_Pool = nullptr;

        Array<uint> m_Generations;
        Array<uint> m_FreeSpaces;

        uint m_Pos = 0;
        uint m_ObjectCount = 0;
        const uint m_MaxObjects;
    };
}

