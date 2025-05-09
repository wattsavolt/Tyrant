#pragma once

#include "Memory/Allocation.h"

namespace tyr
{
    template<typename T, uint C>
    class LocalArray final
    {
    private:
        T m_Data[C];
        uint m_Size;

        void InsertElements(uint index, uint count)
        {
            TYR_ASSERT(m_Size + count < C);
            for (uint i = m_Size; i-- > index;)
            {
                m_Data[i + count] = std::move(m_Data[i]);
            }
            m_Size += count;
        }

    public:
        LocalArray(uint size = 0)
        {
            TYR_ASSERT(size <= C);
            m_Size = size;
        }

        ~LocalArray()
        {
            
        }

        uint Size() const
        {
            return m_Size;
        }

        void Resize(uint size)
        {
            TYR_ASSERT(m_Size <= C);
            m_Size = size;
        }

        // Returns true if the array is empty
        bool IsEmpty() const
        {
            return m_Size == 0;
        }

        const T& operator[](uint index) const
        {
            return m_Data[index];
        }

        T& operator[](uint index)
        {
            return m_Data[index];
        }

        T* Data()
        {
            return m_Data;
        }

        const T* Data() const
        {
            return m_Data;
        }

        // Begin iterator for range-based for loops
        T* begin()
        {
            return m_Data;
        }

        const T* begin() const
        {
            return m_Data;
        }

        // End iterator for range-based for loops
        T* end()
        {
            return m_Data + m_Size;
        }

        const T* end() const
        {
            return m_Data + m_Size;
        }

        bool Contains(const T& value) const
        {
            for (const T& item : *this)
            {
                if (item == value)
                {
                    return true;
                }
            }
            return false;
        }

        void Add(const T& item)
        {
            TYR_ASSERT(m_Size < C);
            m_Data[m_Size++] = item;
        }

        void Add(T&& item)
        {
            TYR_ASSERT(m_Size < C);
            m_Data[m_Size++] = std::move(item);
        }

        T& ExpandOne()
        {
            TYR_ASSERT(m_Size < C);
            return m_Data[m_Size++];
        }

        void Insert(uint index, const T& item)
        {
            TYR_ASSERT(index <= m_Size);
            InsertElements(index, 1);
            m_Data[index] = item;
        }

        void Insert(uint index, T&& item)
        {
            TYR_ASSERT(index <= m_Size);
            InsertElements(index, 1);
            m_Data[index] = std::move(item);
        }

        void Insert(uint index, const T* items, uint count)
        {
            TYR_ASSERT(index <= m_Size);
            TYR_ASSERT(items != nullptr);

            InsertElements(index, count);
            for (uint i = 0; i < count; ++i)
            {
                m_Data[index + i] = items[i];
            }
        }

        // Erase an item by index
        void Erase(uint index)
        {
            TYR_ASSERT(index < m_Size);
            // Move items after the erased item to fill the gap
            for (uint i = index; i < m_Size - 1; ++i)
            {
                m_Data[i] = std::move(m_Data[i + 1]);
            }
            --m_Size;       
        }

        T& Back()
        {
            TYR_ASSERT(m_Size > 0);
            return m_Data[m_Size - 1];
        }

        const T& Back() const
        {
            TYR_ASSERT(m_Size > 0);
            return m_Data[m_Size - 1];
        }

        void PopBack()
        {
            TYR_ASSERT(m_Size > 0);
            --m_Size;
        }

        void Clear()
        {
            m_Size = 0;
        }
    }; 
}

        