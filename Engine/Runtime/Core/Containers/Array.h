#pragma once

#include "Base/Base.h"
#include "Memory/Allocation.h"

namespace tyr
{
    template <typename T>
    constexpr bool IsDefaultConstructibleObject =
        !std::is_abstract_v<T> &&
        !std::is_reference_v<T> &&
        !std::is_pointer_v<T> &&
        std::is_default_constructible_v<T>;

    template<typename T, typename A = HeapAllocator>
    class Array final 
    {
    private:
        T* m_Data;
        uint m_Size;
        uint m_Capacity;

        void DestructElement(uint index)
        {
            if constexpr (!std::is_trivially_destructible<T>::value)
            {
                m_Data[index].~T();
            }
        }

        void CreateElements(uint amount)
        {
            if constexpr (IsDefaultConstructibleObject<T>)
            {
                m_Data = NewN<T, A>(amount);
            }
            else
            {
                m_Data = AllocN<T, A>(amount);
            }
        }

        template<typename... Args>
        void ConstructElements(uint startIndex, uint count, Args&&... args)
        {
            if constexpr (IsDefaultConstructibleObject<T>)
            {
                for (uint i = startIndex; i < startIndex + count; ++i)
                {
                    Construct<T>(&m_Data[i], std::forward<Args>(args)...);
                }
            }
        }

        void DestructElements(uint startIndex, uint count)
        {
            if constexpr (!std::is_trivially_destructible<T>::value)
            {
                for (uint i = startIndex; i < startIndex + count; ++i)
                {
                    m_Data[i].~T();
                }
            }
        }

        // Ensure the array has enough capacity
        void EnsureCapacity(uint capacity)
        {
            if (capacity > m_Capacity)
            {
                const uint newCap = std::max(m_Capacity * 2, capacity);
                T* newData = AllocN<T, A>(newCap);

                // Move existing elements to the new allocation
                for (uint i = 0; i < m_Size; ++i)
                {
                    Construct<T>(&newData[i], std::move(m_Data[i]));
                }

                // Old elemtns still need to be destructed (if non trbiially destructible) even after being moved
                DestructElements(0, m_Size);

                // Free the old data
                Free<A>(m_Data);

                // Update data pointer and capacity
                m_Data = newData;
                m_Capacity = newCap;
            }
        }

        void InsertElements(uint index, uint count)
        {
            EnsureCapacity(m_Size + count);
            for (uint i = m_Size; i-- > index;)
            {
                m_Data[i + count] = std::move(m_Data[i]);
            }
            m_Size += count;
        }

    public:
        Array(uint size = 0)
            : m_Data(nullptr)
        {
            if (size > 0)
            {
                CreateElements(size);
            }
          
            m_Capacity = size;
            m_Size = size;
        }

        ~Array()
        {    
            DestructElements(0, m_Size);
            Free<A>(m_Data);
        }

        Array(Array<T, A>&& other) noexcept
            : m_Data(other.m_Data)
            , m_Capacity(other.m_Capacity)
            , m_Size(other.m_Size)
        {
            other.m_Data = nullptr;
            other.m_Capacity = 0;
            other.m_Size = 0;
        }

        Array(const Array& other)
            : m_Size(other.m_Size)
            , m_Capacity(other.m_Capacity)
        {
            if (m_Capacity > 0)
            {
                m_Data = AllocN<T, A>(m_Capacity);
                for (uint i = 0; i < m_Size; ++i)
                {
                    m_Data[i] = other.m_Data[i];
                }
            }
            else
            {
                m_Data = nullptr;
            }
        }

        Array(std::initializer_list<T> initList)
        {
            m_Size = static_cast<uint>(initList.size());
            m_Capacity = m_Size;

            if (m_Size > 0)
            {
                m_Data = AllocN<T, A>(m_Capacity);
                uint i = 0;
                for (const T& value : initList)
                {
                    Construct<T>(&m_Data[i++], value);
                }
            }
            else
            {
                m_Data = nullptr;
            }
        }

        Array& operator=(const Array& other)
        {
            if (this != &other)
            {
                DestructElements(0, m_Size);
                Free<A>(m_Data);

                m_Size = other.m_Size;
                m_Capacity = other.m_Capacity;

                if (m_Capacity > 0)
                {
                    m_Data = AllocN<T, A>(m_Capacity);
                    for (uint i = 0; i < m_Size; ++i)
                    {
                        m_Data[i] = other.m_Data[i];
                    }
                }
                else
                {
                    m_Data = nullptr;
                }
            }
            return *this;
        }

        void Reserve(uint capacity)
        {
            EnsureCapacity(capacity);
        }

        template<typename... Args>
        void Resize(uint size, Args&&... args)
        {           
            if (size > m_Size)
            {
                EnsureCapacity(size);
                ConstructElements(m_Size, size - m_Size, std::forward<Args>(args)...);
                m_Size = size;
            }
            else if (size < m_Size)
            {
                DestructElements(size, m_Size - size);
                m_Size = size;
            }
        }

        uint Size() const
        {
            return m_Size;
        }

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

        // iterator functions for range-based for loops
        T* begin()
        {
            return m_Data;
        }

        const T* begin() const
        {
            return m_Data;
        }

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
            EnsureCapacity(m_Size + 1);
            Construct<T>(&m_Data[m_Size++], item);
        }

        void Add(T&& item)
        {
            EnsureCapacity(m_Size + 1);
            Construct<T>(&m_Data[m_Size++], std::move(item));
        }

        template<typename... Args>
        void Emplace(Args&&... args)
        {
            EnsureCapacity(m_Size + 1);
            Construct<T>(&m_Data[m_Size++], std::forward<Args>(args)...);
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
            TYR_ASSERT(index < m_Size)          
            // Move items after the erased item to fill the gap
            for (uint i = index; i < m_Size - 1; ++i)
            {
                m_Data[i] = std::move(m_Data[i + 1]);
            }
            // Destruct the last item
            DestructElement(--m_Size);       
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
            DestructElement[--m_Size];
        }

        void Clear()
        {
            DestructElements(0, m_Size);
            m_Size = 0;
        }
    };
}

