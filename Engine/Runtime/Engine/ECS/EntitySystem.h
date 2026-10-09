#pragma once

#include "Core.h"
#include "EngineMacros.h"
#include "ComponentRegistry.h"
#include <bit>

namespace tyr
{
    using Entity = uint;
    constexpr Entity c_InvalidEntity = ~0u;

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

        // Calls func(ComponentTypeID) once for every set bit - skips straight from one
        // set bit to the next instead of checking all c_MaxComponentTypes positions, so
        // this only costs as much as the number of components actually in the mask.
        template<typename Func>
        void ForEachSet(Func&& func) const
        {
            for (uint w = 0; w < c_ComponentMaskWordCount; ++w)
            {
                uint64 word = words[w];
                while (word != 0)
                {
                    const uint bit = static_cast<uint>(std::countr_zero(word));
                    func(w * c_ComponentMaskBitsPerWord + bit);
                    word &= word - 1; // clear the lowest set bit so the next loop finds the next one
                }
            }
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
    // Archetype::entities, the entity IDs themselves) as one flat byte buffer, since a
    // component's real type isn't known here - Archetype::columns has to be an array of
    // the same type for every component slot, so the actual bytes are only ever
    // interpreted as T through At<T>(). The buffer's size covers every row written so far,
    // so the rows are carried over when it grows.
    struct EcsColumn
    {
        Array<uint8> data;
        uint elementSize = 0;

        bool IsInitialized() const
        {
            return elementSize > 0;
        }

        // Rows a column has room for up front, so a typical level's archetypes never need to
        // reallocate. Beyond this the buffer doubles as it grows.
        static constexpr uint c_InitialRowCapacity = 256;

        void Init(uint size)
        {
            TYR_ASSERT(!IsInitialized());
            elementSize = size;
            data.Reserve(c_InitialRowCapacity * size);
        }

        void EnsureCapacity(uint rowCount)
        {
            TYR_ASSERT(IsInitialized());
            // Resized rather than reserved, since growing only copies the bytes within the size.
            const uint byteCount = rowCount * elementSize;
            if (byteCount > data.Size())
            {
                data.Resize(Array<uint8>::UninitializedTag{}, byteCount);
            }
        }

        void* GetElement(uint row)
        {
            TYR_ASSERT(IsInitialized());
            return data.Data() + (size_t)row * elementSize;
        }

        template<typename T>
        T& At(uint row)
        {
            return *static_cast<T*>(GetElement(row));
        }

        // Marks the column as unused again. Deliberately keeps the byte buffer's
        // capacity rather than freeing it - if this slot gets reused for another
        // component type, EnsureCapacity() can reuse the same allocation instead of
        // making a fresh one.
        void Release()
        {
            elementSize = 0;
        }
    };

    struct Archetype
    {
        ArchetypeKey key;
        Handle poolHandle;

        EcsColumn entities;
        EcsColumn columns[c_MaxComponentTypes];

        uint count = 0; // how many rows are actually in use - this is what tells entities and every column how far to read

        // Called by the archetype pool when this slot is freed and might be handed out
        // again for a different set of components. Puts the archetype back to a blank,
        // unused state - without this, a reused slot would still think it has the
        // previous archetype's columns and row count.
        void Reset()
        {
            entities.Release();
            key.ForEachSet([this](uint id)
            {
                columns[id].Release();
            });
            key = ArchetypeKey{};
            poolHandle = Handle{};
            count = 0;
        }
    };

    struct EntityRecord
    {
        Archetype* archetype;
        uint index;
    };

    class TYR_ENGINE_API EntitySystem final
    {
    public:
        EntitySystem();
        ~EntitySystem();

        Entity CreateEntity();

        // Removes the entity and all of its components.
        void RemoveEntity(Entity entity);

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
            ++m_Version;
        }

        template<typename T>
        T& GetComponent(Entity entity)
        {
            EntityRecord& record = m_EntityRecords[entity];
            EcsColumn& column = record.archetype->columns[ComponentRegistry::GetComponentTypeID<T>()];
            return column.At<T>(record.index);
        }

        // The component's bytes, for code that only knows its type by ID.
        void* GetComponentData(Entity entity, ComponentTypeID typeID)
        {
            EntityRecord& record = m_EntityRecords[entity];
            return record.archetype->columns[typeID].GetElement(record.index);
        }

        const void* GetComponentData(Entity entity, ComponentTypeID typeID) const
        {
            const EntityRecord* record = m_EntityRecords.Find(entity);
            return record->archetype->columns[typeID].GetElement(record->index);
        }

        bool HasComponent(Entity entity, ComponentTypeID typeID) const
        {
            const EntityRecord* record = m_EntityRecords.Find(entity);
            return record && record->archetype && record->archetype->key.Test(typeID);
        }

        // Calls func(ComponentTypeID) for each component the entity has.
        template<typename Func>
        void ForEachComponentType(Entity entity, Func&& func) const
        {
            const EntityRecord* record = m_EntityRecords.Find(entity);
            if (record && record->archetype)
            {
                record->archetype->key.ForEachSet(func);
            }
        }

        template<typename T>
        bool HasComponent(Entity entity) const
        {
            const EntityRecord* record = m_EntityRecords.Find(entity);
            return record && record->archetype && record->archetype->key.Test(ComponentRegistry::GetComponentTypeID<T>());
        }

        // Calls func(Entity, T&) once for every entity that has a T, across every archetype
        // that includes it. No react/event system yet - this is a plain, on-demand scan,
        // meant to be called by whatever wants the current set right now (e.g. once at
        // startup for the handful of test entities), not something that runs every frame.
        template<typename T, typename Func>
        void ForEach(Func&& func)
        {
            const ComponentTypeID typeID = ComponentRegistry::GetComponentTypeID<T>();
            for (auto kv : m_Archetypes)
            {
                Archetype* archetype = kv.second;
                if (!archetype->key.Test(typeID))
                {
                    continue;
                }

                EcsColumn& column = archetype->columns[typeID];
                for (uint row = 0; row < archetype->count; ++row)
                {
                    func(archetype->entities.At<Entity>(row), column.At<T>(row));
                }
            }
        }

        // Puts this back to a blank, no-entities state - see World::Reset(), which owns one
        // of these and needs to clear it out along with everything else when a pooled World
        // slot is recycled for a new level.
        void Reset();

        // Changes whenever a component is added or an entity is removed.
        uint GetVersion() const { return m_Version; }

    private:
        Entity m_NextEntityID{ 0 };
        uint m_Version = 0;

        HashMap<Entity, EntityRecord> m_EntityRecords;
        HashMap<ArchetypeKey, Archetype*> m_Archetypes;

        LocalObjectPool<Archetype, c_MaxArchetypes, ResetObjectPolicy> m_ArchetypePool;

        Archetype* GetOrCreateArchetype(const ArchetypeKey& key);
        void MoveEntity(Entity e, EntityRecord& record, Archetype* newArch);
        void RemoveFromArchetype(EntityRecord& record);
        void CopyComponents(const EntityRecord& from, Archetype* to, uint toIndex);

        template<typename T>
        void AddTypeToKey(ArchetypeKey& key)
        {
            key.Set(ComponentRegistry::GetComponentTypeID<T>());
        }
    };
}
