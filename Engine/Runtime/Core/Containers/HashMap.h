#pragma once

#include "Array.h"
#include "Math/Math.h"
#include "Utility/Utility.h"

namespace tyr
{
    // Hash map that uses linear probing for collisions and keeps the load factor at 50% max
    template <typename Key, typename Value, bool FixedCapacity = false, typename Hash = std::hash<Key>, typename A = HeapAllocator>
    class HashMap final
    {
    private:
        struct Bucket
        {
            Key key;
            Value value;
            bool occupied = false;
            bool deleted = false;
        };

        Array<Bucket, A> m_Buckets;
        uint m_Capacity;
        uint m_Size;

        uint ProbeIndex(uint hash, uint i) const
        {
            return (hash + i) & (m_Capacity - 1); 
        }

        void Rehash(uint newCapacity)
        {
            Array<Bucket, A> newBuckets;
            newBuckets.Resize(newCapacity);
            for (uint i = 0; i < newCapacity; ++i)
            {
                newBuckets[i].occupied = false;
                newBuckets[i].deleted = false;
            }

            for (uint i = 0; i < m_Capacity; ++i)
            {
                if (m_Buckets[i].occupied && !m_Buckets[i].deleted)
                {
                    Key& key = m_Buckets[i].key;
                    Value& value = m_Buckets[i].value;
                    const uint hash = static_cast<uint>(Hash{}(key));

                    for (uint j = 0; j < newCapacity; ++j)
                    {
                        const uint index = (hash + j) & (newCapacity - 1);
                        if (!newBuckets[index].occupied)
                        {
                            newBuckets[index].key = std::move(key);
                            newBuckets[index].value = std::move(value);
                            newBuckets[index].occupied = true;
                            break;
                        }
                    }
                }
            }

            m_Buckets = std::move(newBuckets);
            m_Capacity = newCapacity;
        }

        void EnsureCapacity(uint requiredSize)
        {
            const uint requiredCapacity = requiredSize * c_InvLoadFactor;

            if (requiredCapacity > m_Capacity)
            {
                if constexpr (FixedCapacity)
                {
                    TYR_LOG_FATAL("Cannot increase capacity of a fixed capacity hash map.");
                }
                Rehash(Math::NextPowerOfTwo(requiredCapacity));
            }
        }

        const Value* FindInternal(const Key& key) const
        {
            const uint hash = Hash{}(key);
            for (uint i = 0; i < m_Capacity; ++i)
            {
                const uint index = ProbeIndex(hash, i);
                const Bucket& bucket = m_Buckets[index];

                if (!bucket.occupied && !bucket.deleted)
                {
                    return nullptr;
                }

                if (bucket.occupied && !bucket.deleted && bucket.key == key)
                {
                    return &bucket.value;
                }
            }
            return nullptr;
        }


    public:
        static constexpr float c_InvLoadFactor = 1 / 0.5f;

        HashMap(uint capacity = 8)
            : m_Capacity(Math::NextPowerOfTwo(capacity * c_InvLoadFactor))
            , m_Size(0)
        {
            m_Buckets.Resize(m_Capacity);
            for (uint i = 0; i < m_Capacity; ++i)
            {
                m_Buckets[i].occupied = false;
                m_Buckets[i].deleted = false;
            }
        }

        ~HashMap() = default;

        Value& operator[](const Key& key)
        {
            uint hash = Hash{}(key);
            for (uint i = 0; i < m_Capacity; ++i)
            {
                const uint index = ProbeIndex(hash, i);
                Bucket& bucket = m_Buckets[index];

                if (bucket.occupied && !bucket.deleted && bucket.key == key)
                {
                    return bucket.value;
                }

                if (!bucket.occupied || bucket.deleted)
                {
                    EnsureCapacity(m_Size + 1); // Ensure BEFORE insert!
                    hash = static_cast<uint>(Hash{}(key)); // In case rehash changed capacity

                    for (uint j = 0; j < m_Capacity; ++j)
                    {
                        uint insertIndex = ProbeIndex(hash, j);
                        Bucket& insertBucket = m_Buckets[insertIndex];
                        if (!insertBucket.occupied || insertBucket.deleted)
                        {
                            insertBucket.key = key;
                            insertBucket.value = Value();
                            insertBucket.occupied = true;
                            insertBucket.deleted = false;
                            ++m_Size;
                            return insertBucket.value;
                        }
                    }
                }
            }

            TYR_ASSERT(false);
            static Value dummy = {};
            return dummy;
        }

        void Insert(const Key& key, const Value& value)
        {
            EnsureCapacity(m_Size + 1);
            uint hash = Hash{}(key);

            for (uint i = 0; i < m_Capacity; ++i)
            {
                uint index = ProbeIndex(hash, i);
                Bucket& bucket = m_Buckets[index];

                if (bucket.occupied && !bucket.deleted && bucket.key == key)
                {
                    bucket.value = value;
                    return;
                }

                if (!bucket.occupied || bucket.deleted)
                {
                    bucket.key = key;
                    bucket.value = value;
                    bucket.occupied = true;
                    bucket.deleted = false;
                    ++m_Size;
                    return;
                }
            }

            TYR_ASSERT(false);
        }

        Value* Find(const Key& key)
        {
            return const_cast<Value*>(static_cast<const HashMap*>(this)->FindInternal(key));
        }

        const Value* Find(const Key& key) const
        {
            return FindInternal(key);
        }

        bool Contains(const Key& key) const
        {
            return FindInternal(key) != nullptr;
        }
        
        void Erase(const Key& key)
        {
            const uint hash = static_cast<uint>(Hash{}(key));
            for (uint i = 0; i < m_Capacity; ++i)
            {
                uint index = ProbeIndex(hash, i);
                Bucket& bucket = m_Buckets[index];

                if (!bucket.occupied && !bucket.deleted)
                {
                    return;
                }

                if (bucket.occupied && !bucket.deleted && bucket.key == key)
                {
                    bucket.deleted = true;
                    --m_Size;
                    return;
                }
            }
        }

        void Reserve(uint n)
        {
            EnsureCapacity(n);
        }

        // Keeps every bucket alive (never destructs a key/value - the same way Erase()
        // never does either, they're just marked unused) and keeps the bucket array's own
        // capacity, so reusing this HashMap after Clear() never needs to allocate again.
        void Clear()
        {
            for (uint i = 0; i < m_Capacity; ++i)
            {
                m_Buckets[i].occupied = false;
                m_Buckets[i].deleted = false;
            }
            m_Size = 0;
        }

        uint Size() const { return m_Size; }

        uint Capacity() const { return m_Capacity; }

        // Iterator classes
        class Iterator
        {
        public:
            Iterator(Bucket* ptr, Bucket* end)
                : m_Ptr(ptr), m_End(end)
            {
                AdvanceToValid();
            }

            Iterator& operator++()
            {
                ++m_Ptr;
                AdvanceToValid();
                return *this;
            }

            std::pair<const Key&, Value&> operator*() 
            {
                return { m_Ptr->key, m_Ptr->value };
            }

            bool operator!=(const Iterator& other) const
            {
                return m_Ptr != other.m_Ptr;
            }

        private:
            Bucket* m_Ptr;
            Bucket* m_End;

            void AdvanceToValid()
            {
                while (m_Ptr < m_End && (!m_Ptr->occupied || m_Ptr->deleted))
                {
                    ++m_Ptr;
                }
            }
        };

        class ConstIterator
        {
        public:
            ConstIterator(const Bucket* ptr, const Bucket* end)
                : m_Ptr(ptr), m_End(end)
            {
                AdvanceToValid();
            }

            ConstIterator& operator++()
            {
                ++m_Ptr;
                AdvanceToValid();
                return *this;
            }

            std::pair<const Key&, const Value&> operator*() const
            {
                return { m_Ptr->key, m_Ptr->value };
            }

            bool operator!=(const ConstIterator& other) const
            {
                return m_Ptr != other.m_Ptr;
            }

        private:
            const Bucket* m_Ptr;
            const Bucket* m_End;

            void AdvanceToValid()
            {
                while (m_Ptr < m_End && (!m_Ptr->occupied || m_Ptr->deleted))
                {
                    ++m_Ptr;
                }
            }
        };
        
        // Each non-const iterator resolves to a key value pair copy with references inside it. The value is a non-const reference so it can be modified
        Iterator begin() { return Iterator(m_Buckets.Data(), m_Buckets.Data() + m_Capacity); }
        Iterator end() { return Iterator(m_Buckets.Data() + m_Capacity, m_Buckets.Data() + m_Capacity); }

        ConstIterator begin() const { return ConstIterator(m_Buckets.Data(), m_Buckets.Data() + m_Capacity); }
        ConstIterator end() const { return ConstIterator(m_Buckets.Data() + m_Capacity, m_Buckets.Data() + m_Capacity); }

        ConstIterator cbegin() const { return begin(); }
        ConstIterator cend() const { return end(); }
    };
	
}

