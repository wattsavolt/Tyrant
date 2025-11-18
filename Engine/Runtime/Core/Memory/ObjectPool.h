#pragma once

#include "Base/Base.h"
#include "Containers/Array.h"
#include "Threading/Threading.h"

namespace tyr
{
    template<class T>
    class ObjectPool final
    {
    public:
        ObjectPool(uint16 maxObjects)
            : m_MaxObjects(maxObjects)
        {
            m_Pool = AllocN<T>(maxObjects);
            m_FreeSpaces.Reserve(m_MaxObjects);
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
            uint16 index;
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

        uint16 GetIndex(const T* object) const
        {
            TYR_ASSERT(object != nullptr && m_ObjectCount > 0);
            return object - m_Pool;
        }

        const T* GetObject(uint16 index) const
        {
            return m_Pool[index];
        }

        T* GetObject(uint16 index)
        {
            return m_Pool[index];
        }

        const T& GetObjectRef(uint16 index) const
        {
            return m_Pool[index];
        }

        T& GetObjectRef(uint16 index)
        {
            return m_Pool[index];
        }

        void Delete(T* object)
        {
            TYR_ASSERT(object != nullptr && m_ObjectCount > 0);
            const uint16 index = object - m_Pool;
            m_FreeSpaces.Add(index);
            object->~T();
            m_ObjectCount--;
        }

        void Delete(uint16 index)
        {
            Delete(GetObject(index));
        }

    private:
        T* m_Pool;
        Array<uint16> m_FreeSpaces;
        uint16 m_Pos = 0;
        uint16 m_ObjectCount = 0;
        const uint16 m_MaxObjects;
    };
}

