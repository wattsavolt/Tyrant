#include "EntitySystem.h"

namespace tyr
{
    EntitySystem::EntitySystem()
    {
    }

    EntitySystem::~EntitySystem()
    {
        for (auto kv : m_Archetypes)
        {
            Archetype* arch = kv.second;

            arch->entities.Release();
            for (uint i = 0; i < c_MaxComponentTypes; ++i)
            {
                arch->columns[i].Release();
            }

            m_ArchetypePool.Delete(arch->poolHandle);
        }

        m_Archetypes.Clear();
    }

    Entity EntitySystem::CreateEntity()
    {
        Entity e = m_NextEntityID++;
        m_EntityRecords[e] = {};
        return e;
    }

    Archetype* EntitySystem::GetOrCreateArchetype(const ArchetypeKey& key)
    {
        if (Archetype** existing = m_Archetypes.Find(key))
            return *existing;

        Handle h = m_ArchetypePool.Create();
        Archetype* arch = &m_ArchetypePool[h];
        arch->key = key;
        arch->poolHandle = h;

        m_Archetypes[key] = arch;
        return arch;
    }

    void EntitySystem::MoveEntity(Entity e, EntityRecord& record, Archetype* newArch)
    {
        const uint newIndex = newArch->count;

        if (!newArch->entities.IsInitialized())
        {
            newArch->entities.Init(sizeof(Entity));
        }
        newArch->entities.EnsureCapacity(newIndex + 1);
        newArch->entities.At<Entity>(newIndex) = e;
        newArch->count++;

        if (record.archetype)
        {
            CopyComponents(record, newArch);
            RemoveFromArchetype(record);
        }

        record.archetype = newArch;
        record.index = newIndex;
    }

    void EntitySystem::RemoveFromArchetype(EntityRecord& record)
    {
        Archetype* arch = record.archetype;
        const uint index = record.index;
        const uint last = arch->count - 1;

        if (index != last)
        {
            Entity moved = arch->entities.At<Entity>(last);
            arch->entities.At<Entity>(index) = moved;

            m_EntityRecords[moved].index = index;

            // TODO: once CopyComponents can actually copy component data, do this same
            // swap for every column that's in use here too (moved <-> index), just like
            // we did for entities above, so every column still lines up row for row.
        }

        arch->count--;
    }

    void EntitySystem::CopyComponents(const EntityRecord& from, Archetype* to)
    {
        // TODO: for every column that's in use on from.archetype, set up the matching
        // column on `to` (Init it if it isn't already, make sure it has room), then
        // copy the entity's data at from.index into the new row.
    }
}
