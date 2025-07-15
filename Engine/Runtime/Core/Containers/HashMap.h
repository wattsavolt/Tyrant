#pragma once

#include "Array.h"
#include "Math/Math.h"

namespace tyr
{
    // Hash map thast uses linear probing for collisions and keeps the load factor at 50% max
    template <typename Key, typename Value, typename Hash = std::hash<Key>, typename A = HeapAllocator>
    class HashMap
    {
    private:
        struct Bucket
        {
            Key Key;
            Value Value;
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
                    const Key& key = m_Buckets[i].Key;
                    const Value& value = m_Buckets[i].Value;
                    uint hash = Hash{}(key);

                    for (uint j = 0; j < newCapacity; ++j)
                    {
                        uint index = (hash + j) & (newCapacity - 1);
                        if (!newBuckets[index].occupied)
                        {
                            newBuckets[index].Key = key;
                            newBuckets[index].Value = value;
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
            // Make sure a max load factor of 0.5 is kept
            uint requiredCapacity = requiredSize * 2;
            if (requiredCapacity > m_Capacity)
            {
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

                if (bucket.occupied && !bucket.deleted && bucket.Key == key)
                {
                    return &bucket.Value;
                }
            }
            return nullptr;
        }


    public:
        HashMap(uint capacity = 8)
            : m_Capacity(Math::NextPowerOfTwo(capacity))
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
                uint index = ProbeIndex(hash, i);
                Bucket& bucket = m_Buckets[index];

                if (bucket.occupied && !bucket.deleted && bucket.Key == key)
                {
                    return bucket.Value;
                }

                if (!bucket.occupied || bucket.deleted)
                {
                    EnsureCapacity(m_Size + 1); // Ensure BEFORE insert!
                    hash = Hash{}(key); // In case rehash changed capacity

                    for (uint j = 0; j < m_Capacity; ++j)
                    {
                        uint insertIndex = ProbeIndex(hash, j);
                        Bucket& insertBucket = m_Buckets[insertIndex];
                        if (!insertBucket.occupied || insertBucket.deleted)
                        {
                            insertBucket.Key = key;
                            insertBucket.Value = Value();
                            insertBucket.occupied = true;
                            insertBucket.deleted = false;
                            ++m_Size;
                            return insertBucket.Value;
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

                if (bucket.occupied && !bucket.deleted && bucket.Key == key)
                {
                    bucket.Value = value;
                    return;
                }

                if (!bucket.occupied || bucket.deleted)
                {
                    bucket.Key = key;
                    bucket.Value = value;
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
            uint hash = Hash{}(key);
            for (uint i = 0; i < m_Capacity; ++i)
            {
                uint index = ProbeIndex(hash, i);
                Bucket& bucket = m_Buckets[index];

                if (!bucket.occupied && !bucket.deleted)
                {
                    return;
                }

                if (bucket.occupied && !bucket.deleted && bucket.Key == key)
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

        void Clear()
        {
            m_Buckets.Clear();
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

            std::pair<const Key&, Value&> operator*() const
            {
                return { m_Ptr->Key, m_Ptr->Value };
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
                return { m_Ptr->Key, m_Ptr->Value };
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

        Iterator begin() { return Iterator(m_Buckets.Data(), m_Buckets.Data() + m_Capacity); }
        Iterator end() { return Iterator(m_Buckets.Data() + m_Capacity, m_Buckets.Data() + m_Capacity); }

        ConstIterator begin() const { return ConstIterator(m_Buckets.Data(), m_Buckets.Data() + m_Capacity); }
        ConstIterator end() const { return ConstIterator(m_Buckets.Data() + m_Capacity, m_Buckets.Data() + m_Capacity); }

        ConstIterator cbegin() const { return begin(); }
        ConstIterator cend() const { return end(); }
    };
	
}

