#pragma once

#include "Base/Base.h"
#include "Containers/LocalArray.h"

namespace tyr
{
    // Local object pool that stores a C-style array of objects.
    template<class T, uint N, bool ClearOnFree = true>
    class LocalObjectPool final
    {
    public:
        LocalObjectPool() = default;
       
        ~LocalObjectPool()
        {
            TYR_ASSERT(m_ObjectCount == 0);
        }

        T* Create(uint& index)
        {
            TYR_ASSERT(m_ObjectCount < N);
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
            return &m_Pool[index];
        }

        uint GetIndex(const T* object) const
        {
            TYR_ASSERT(object != nullptr && m_ObjectCount > 0);
            return object - m_Pool;
        }

        const T* GetObject(uint index) const
        {
            return &m_Pool[index];
        }

        T* GetObject(uint index)
        {
            return &m_Pool[index];
        }

        const T& GetObjectRef(uint index) const
        {
            return m_Pool[index];
        }

        T& GetObjectRef(uint index)
        {
            return m_Pool[index];
        }

        void Delete(T* object)
        {
            TYR_ASSERT(object != nullptr && m_ObjectCount > 0);
            const uint16 index = object - m_Pool;
            m_FreeSpaces.Add(index);
            m_ObjectCount--;
            if constexpr (ClearOnFree) 
            {
                *object = {};
            }
        }

        void Delete(uint index)
        {
            Delete(&m_Pool[index]);
        }

    private:
        T m_Pool[N];
        LocalArray<uint, N> m_FreeSpaces;
        uint m_Pos = 0;
        uint m_ObjectCount = 0;
    };
}

