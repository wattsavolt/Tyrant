#pragma once

#include "Array.h"

namespace tyr
{
    //template <typename Key, typename Value, typename Hash = std::hash<Key>, typename A = HeapAllocator>
    //class FixedHashMap 
    //{
    //private:
    //    using Bucket = std::pair<const Key, Value>;
    //    Bucket* m_Buckets; // Dynamic array of buckets
    //    size_t m_Capacity; // Number of buckets
    //    size_t m_Size; // Number of elements in the map

    //    // Function to get the nearest power of 2 greater than or equal to n
    //    size_t NextPowerOfTwo(size_t n) 
    //    {
    //        if (n == 0) return 1;
    //        return std::pow(2, std::ceil(std::log2(n)));
    //    }

    //    // Rehash the map to a new capacity
    //    void Rehash(size_t newCapacity) 
    //    {
    //        auto newBuckets = NewN<Bucket, A>(newCapacity); // Allocate new bucket array
    //        // Rehash existing elements into the new buckets
    //        for (size_t i = 0; i < m_Capacity; ++i) 
    //        {
    //            for (auto& item : m_Buckets[i]) 
    //            {
    //                size_t index = Hash{}(item.first) % newCapacity;
    //                newBuckets[index].Add(item); // Add the item to the new bucket
    //            }
    //        }
    //        FreeN(m_Buckets, m_Capacity); // Free old bucket array
    //        m_Buckets = newBuckets;
    //        m_Capacity = newCapacity;
    //    }

    //public:
    //    // Constructor
    //    HashMap(size_t capacity = 8)
    //        : m_Capacity(NextPowerOfTwo(capacity)), m_Size(0) 
    //    {
    //        m_Buckets = NewN<Bucket, A>(m_Capacity); // Allocate bucket array
    //    }

    //    // Destructor to clean up allocated memory
    //    ~HashMap() {
    //        FreeN(m_Buckets, m_Capacity); // Free bucket array
    //    }

    //    // Add or access a key-value pair using operator[]
    //    Value& operator[](const Key& key) 
    //    {
    //        size_t index = Hash{}(key) % m_Capacity;
    //        for (auto& item : m_Buckets[index]) 
    //        {
    //            if (item.first == key) 
    //            {
    //                return item.second; // Return existing value
    //            }
    //        }
    //        // If not found, create a new entry
    //        m_Buckets[index].Add({ key, Value() }); // Default-constructed Value
    //        ++m_Size;
    //        return m_Buckets[index].Back().second; // Return reference to the new value
    //    }

    //    // Add a key-value pair
    //    void Insert(const Key& key, const Value& value)
    //    {
    //        size_t index = Hash{}(key) % m_Capacity;
    //        if (!m_Buckets[index].Contains({ key, value })) { // Avoid duplicates
    //            m_Buckets[index].Add({ key, value });
    //            ++m_Size;

    //            // Check if rehashing is needed
    //            if (m_Size / m_Capacity > 1) 
    //            { // Load factor > 1
    //                Rehash(m_Capacity * 2);
    //            }
    //        }
    //    }

    //    // Find value by key
    //    Value* Find(const Key& key) 
    //    {
    //        size_t index = Hash{}(key) % m_Capacity;
    //        for (auto& item : m_Buckets[index]) 
    //        {
    //            if (item.first == key) 
    //            {
    //                return &item.second; // Return a pointer to the value
    //            }
    //        }
    //        return nullptr; // Not found
    //    }

    //    // Remove a key-value pair
    //    void Erase(const Key& key) 
    //    {
    //        size_t index = Hash{}(key) % m_Capacity;
    //        m_Buckets[index].Erase({ key, Value() }); // Assuming Erase takes a pair
    //        --m_Size;
    //    }

    //    // Reserve space for a certain number of elements
    //    void Reserve(size_t n) 
    //    {
    //        if (n > m_Capacity) 
    //        {
    //            Rehash(NextPowerOfTwo(n));
    //        }
    //    }

    //    size_t Size() const { return m_Size; }
    //    size_t Capacity() const { return m_Capacity; }
    //};
	
}

