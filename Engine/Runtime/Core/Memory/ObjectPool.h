#pragma once

#include "Containers/Array.h"
#include "Threading/Threading.h"

namespace tyr
{
    template<class T>
    class ObjectPool final
    {
    public:
        ObjectPool(uint maxObjects)
            : m_MaxObjects(maxObjects)
        {
            m_Pool = AllocN<T>(maxObjects);
        }

        ~ObjectPool()
        {
            TYR_ASSERT(m_ObjectCount == 0);
            Free(m_Pool);
        }

        template<class ...Args>
        T* Create(Args&&... args) 
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
            return object;
        }

        template<class ...Args>
        T* CreateSafe(Args&&... args)
        {
            LockGuard guard(m_Mutex);
            return Create(std::forward<Args>(args)...);
        }

        uint GetIndex(const T* object) const
        {
            TYR_ASSERT(object != nullptr && m_ObjectCount > 0);
            return object - m_Pool;
        }

        const T* GetObject(uint index) const
        {
            return m_Pool[index];
        }

        T* GetObject(uint index)
        {
            return m_Pool[index];
        }

        void Delete(T* object)
        {
            TYR_ASSERT(object != nullptr && m_ObjectCount > 0);
            const uint index = object - m_Pool;
            m_FreeSpaces.Add(index);
            object->~T();
            m_ObjectCount--;
        }

        void Delete(uint index)
        {
            Delete(GetObject(index));
        }

        void DeleteSafe(T* object)
        {
            LockGuard guard(m_Mutex);
            Delete(object);
        }

        void DeleteSafe(uint index)
        {
            LockGuard guard(m_Mutex);
            Delete(index);
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

