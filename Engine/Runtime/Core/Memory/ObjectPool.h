#pragma once

#include "Containers/DynamicArray.h"
#include "Threading/Threading.h"

namespace tyr
{
    template<class T>
    class ObjectPool 
    {
    public:
        ObjectPool(uint maxObjects)
            : m_MaxObjects(maxObjects)
        {
            m_Pool = AllocN<T>(maxObjects);
        }

        template<class ...Args>
        T* Create(Args&&... args) 
        {
            TYR_ASSERT(m_ObjectCount < maxObjects);
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
            return object;
        }

        template<class ...Args>
        T* CreateSafe(Args&&... args)
        {
            LockGuard guard(m_Mutex);
            return Create(std::forward<Args>(args)...);
        }

    private:
        T* m_Pool;
        Array<uint> m_FreeSpaces;
        uint m_Pos = 0;
        uint m_ObjectCount = 0;
        const uint m_MaxObjects;
        Mutex m_Mutex;
    };
}

