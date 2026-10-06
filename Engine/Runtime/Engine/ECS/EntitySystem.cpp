#include "EntitySystem.h"
#include <cstring>

namespace tyr
{
    EntitySystem::EntitySystem()
    {
    }

    EntitySystem::~EntitySystem()
    {
        Reset();
    }

    void EntitySystem::Reset()
    {
        // Delete() calls Archetype::Reset() itself (see m_ArchetypePool's
        // ResetObjectPolicy), which releases entities/columns.
        for (auto kv : m_Archetypes)
        {
            m_ArchetypePool.Delete(kv.second->poolHandle);
        }

        m_Archetypes.Clear();
        m_EntityRecords.Clear();
        m_NextEntityID = 0;
        m_Version = 0;
    }

    Entity EntitySystem::CreateEntity()
    {
        Entity e = m_NextEntityID++;
        m_EntityRecords[e] = {};
        return e;
    }

    void EntitySystem::RemoveEntity(Entity entity)
    {
        EntityRecord* record = m_EntityRecords.Find(entity);
        TYR_ASSERT(record);
        if (record->archetype)
        {
            RemoveFromArchetype(*record);
        }
        m_EntityRecords.Erase(entity);
        ++m_Version;
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
            CopyComponents(record, newArch, newIndex);
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

            // Every component column needs the same swap as entities above, or a
            // surviving row's data would end up misaligned with which entity it belongs
            // to. Safe as a raw memcpy - see ComponentReflection.h's comment: a component
            // type is never allowed to own a dynamic allocation, so there's nothing for a
            // real copy/move/destructor to do that a byte copy doesn't already do.
            arch->key.ForEachSet([&](uint id)
            {
                EcsColumn& column = arch->columns[id];
                memcpy(column.GetElement(index), column.GetElement(last), column.elementSize);
            });
        }

        arch->count--;
    }

    void EntitySystem::CopyComponents(const EntityRecord& from, Archetype* to, uint toIndex)
    {
        if (!from.archetype)
        {
            return;
        }

        // Carries every component the entity already had over to its new archetype's row -
        // the new component AddComponent<T> is actually adding gets written into its own
        // column separately, after this returns. Raw memcpy is safe for the same reason as
        // RemoveFromArchetype above.
        from.archetype->key.ForEachSet([&](uint id)
        {
            EcsColumn& srcColumn = from.archetype->columns[id];
            EcsColumn& dstColumn = to->columns[id];

            if (!dstColumn.IsInitialized())
            {
                dstColumn.Init(srcColumn.elementSize);
            }
            dstColumn.EnsureCapacity(toIndex + 1);

            memcpy(dstColumn.GetElement(toIndex), srcColumn.GetElement(from.index), srcColumn.elementSize);
        });
    }
}
