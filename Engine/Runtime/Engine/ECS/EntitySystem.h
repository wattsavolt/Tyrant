#pragma once

#include "Core.h"
#include "EngineMacros.h"
#include "ComponentRegistry.h"
#include "EcsChunkPool.h"

namespace tyr
{
    using Entity = uint;

    constexpr uint c_ComponentMaskBitsPerWord = 64;
    constexpr uint c_ComponentMaskWordCount = (c_MaxComponentTypes + c_ComponentMaskBitsPerWord - 1) / c_ComponentMaskBitsPerWord;

    // A fixed set of bits, one per possible component type - setting a bit says "this
    // archetype has that component". No sorting needed, no heap allocation, and
    // comparing two masks is always just as fast no matter how many components an
    // entity actually has.
    struct ComponentMask
    {
        // Up to 64 components per word
        uint64 words[c_ComponentMaskWordCount] = {};

        void Set(ComponentTypeID id)
        {
            TYR_ASSERT(id < c_MaxComponentTypes);
            words[id / c_ComponentMaskBitsPerWord] |= (uint64(1) << (id % c_ComponentMaskBitsPerWord));
        }

        void Clear(ComponentTypeID id)
        {
            TYR_ASSERT(id < c_MaxComponentTypes);
            words[id / c_ComponentMaskBitsPerWord] &= ~(uint64(1) << (id % c_ComponentMaskBitsPerWord));
        }

        bool Test(ComponentTypeID id) const
        {
            TYR_ASSERT(id < c_MaxComponentTypes);
            return (words[id / c_ComponentMaskBitsPerWord] & (uint64(1) << (id % c_ComponentMaskBitsPerWord))) != 0;
        }

        bool operator==(const ComponentMask& other) const
        {
            for (uint i = 0; i < c_ComponentMaskWordCount; ++i)
            {
                if (words[i] != other.words[i])
                {
                    return false;
                }
            }
            return true;
        }

        bool operator!=(const ComponentMask& other) const
        {
            return !(*this == other);
        }
    };

    using ArchetypeKey = ComponentMask;
}

namespace std
{
    template<>
    struct hash<tyr::ComponentMask>
    {
        size_t operator()(const tyr::ComponentMask& mask) const noexcept
        {
            size_t seed = 0;
            for (tyr::uint i = 0; i < tyr::c_ComponentMaskWordCount; ++i)
            {
                tyr::HashCombine(seed, std::hash<tyr::uint64>{}(mask.words[i]));
            }
            return seed;
        }
    };
}

namespace tyr
{
    // The most archetypes we'll ever have alive at the same time. Archetypes come
    // from a pool (see EntitySystem::m_ArchetypePool) instead of being allocated on
    // the heap one at a time.
    constexpr uint c_MaxArchetypes = 128;

    // Holds one archetype's data for a single component type (or, for
    // Archetype::entities, the entity IDs themselves), stored as a list of fixed-size
    // 16KB chunks instead of one big array. When it needs more room it just grabs
    // another chunk from the shared pool - it never has to copy the data that's
    // already there. Chunks are shared across every archetype, so a chunk one
    // archetype stops needing can be picked up and reused by another.
    struct EcsColumn
    {
        Array<Handle> chunkHandles;
        uint elementSize = 0;
        uint elementsPerChunk = 0;

        bool IsInitialized() const
        {
            return elementSize > 0;
        }

        void Init(uint size)
        {
            TYR_ASSERT(!IsInitialized());
            elementSize = size;
            elementsPerChunk = uint(EcsChunk::c_ChunkSizeBytes / size);
            TYR_ASSERT(elementsPerChunk > 0); // a single element must fit in one chunk
        }

        void EnsureCapacity(uint rowCount)
        {
            TYR_ASSERT(IsInitialized());
            const uint requiredChunks = (rowCount + elementsPerChunk - 1) / elementsPerChunk;
            const uint currentSize = chunkHandles.Size();
            for (uint i = currentSize; i < requiredChunks; ++i)
            {
                chunkHandles.Add(EcsChunkPool::Instance().Create());
            }
        }

        void* GetElement(uint row)
        {
            TYR_ASSERT(IsInitialized());
            const uint chunkIndex = row / elementsPerChunk;
            const uint indexInChunk = row % elementsPerChunk;
            EcsChunk& chunk = EcsChunkPool::Instance()[chunkHandles[chunkIndex]];
            return chunk.data + (size_t)indexInChunk * elementSize;
        }

        template<typename T>
        T& At(uint row)
        {
            return *static_cast<T*>(GetElement(row));
        }

        // Gives every chunk back to the shared pool and clears the column out. Don't
        // use the column again until Init() is called on it, if it ends up getting
        // reused for a different component type.
        void Release()
        {
            for (Handle h : chunkHandles)
            {
                EcsChunkPool::Instance().Delete(h);
            }
            chunkHandles.Clear();
            elementSize = 0;
            elementsPerChunk = 0;
        }
    };

    struct Archetype
    {
        ArchetypeKey key;
        Handle poolHandle;

        EcsColumn entities;
        EcsColumn columns[c_MaxComponentTypes];

        uint count = 0; // how many rows are actually in use - this is what tells entities and every column how far to read
    };

    struct EntityRecord
    {
        Archetype* archetype;
        uint index;
    };

    class EntitySystem final
    {
    public:
        EntitySystem();
        ~EntitySystem();

        Entity CreateEntity();

        template<typename T, typename... Args>
        void AddComponent(Entity entity, Args&&... args)
        {
            EntityRecord& record = m_EntityRecords[entity];

            ArchetypeKey newKey = record.archetype ? record.archetype->key : ArchetypeKey{};
            AddTypeToKey<T>(newKey);

            Archetype* newArch = GetOrCreateArchetype(newKey);

            MoveEntity(entity, record, newArch);

            EcsColumn& column = newArch->columns[ComponentRegistry::GetComponentTypeID<T>()];
            if (!column.IsInitialized())
            {
                column.Init(sizeof(T));
            }
            column.EnsureCapacity(record.index + 1);

            column.At<T>(record.index) = T(std::forward<Args>(args)...);
        }

        template<typename T>
        T& GetComponent(Entity entity)
        {
            EntityRecord& record = m_EntityRecords[entity];
            EcsColumn& column = record.archetype->columns[ComponentRegistry::GetComponentTypeID<T>()];
            return column.At<T>(record.index);
        }

    private:
        Entity m_NextEntityID{ 0 };

        HashMap<Entity, EntityRecord> m_EntityRecords;
        HashMap<ArchetypeKey, Archetype*> m_Archetypes;

        LocalObjectPool<Archetype, c_MaxArchetypes, false> m_ArchetypePool;

        Archetype* GetOrCreateArchetype(const ArchetypeKey& key);
        void MoveEntity(Entity e, EntityRecord& record, Archetype* newArch);
        void RemoveFromArchetype(EntityRecord& record);
        void CopyComponents(const EntityRecord& from, Archetype* to);

        template<typename T>
        void AddTypeToKey(ArchetypeKey& key)
        {
            key.Set(ComponentRegistry::GetComponentTypeID<T>());
        }
    };
}
