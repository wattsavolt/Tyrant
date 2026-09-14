#pragma once

#include "Core.h"
#include "EngineMacros.h"
#include "ComponentRegistry.h"
#include <bit>

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
    // interpreted as T through At<T>(). Growing just reserves more of the same buffer
    // (Array<uint8>::Reserve never shrinks or moves data unnecessarily), so this only
    // ever needs to allocate more when it actually runs out of room.
    struct EcsColumn
    {
        Array<uint8> data;
        uint elementSize = 0;

        bool IsInitialized() const
        {
            return elementSize > 0;
        }

        void Init(uint size)
        {
            TYR_ASSERT(!IsInitialized());
            elementSize = size;
        }

        void EnsureCapacity(uint rowCount)
        {
            TYR_ASSERT(IsInitialized());
            data.Reserve((size_t)rowCount * elementSize);
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

        LocalObjectPool<Archetype, c_MaxArchetypes, ResetObjectPolicy> m_ArchetypePool;

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
