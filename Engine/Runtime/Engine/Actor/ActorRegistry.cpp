#include "ActorRegistry.h"
#include "ECS/Components.h"

namespace tyr
{
	ActorRegistry& ActorRegistry::Instance()
	{
		static ActorRegistry registry;
		return registry;
	}

	ActorTypeDesc& ActorRegistry::RegisterActorType(const Id64& typeId, const char* name)
	{
		TYR_ASSERT(!m_ActorTypes.Contains(typeId));

		ActorTypeDesc desc{};
		desc.typeId = typeId;
		desc.name = name;
		m_ActorTypes.Insert(typeId, desc);
		return *m_ActorTypes.Find(typeId);
	}

	const ActorTypeDesc* ActorRegistry::FindActorType(const Id64& typeId) const
	{
		return m_ActorTypes.Find(typeId);
	}

	LocalArray<Entity, c_MaxActorTypeEntities> ActorRegistry::InstantiateActor(const Id64& actorTypeId, EntitySystem& entities) const
	{
		const ActorTypeDesc* desc = FindActorType(actorTypeId);
		TYR_ASSERT(desc != nullptr);

		LocalArray<Entity, c_MaxActorTypeEntities> result;

		// Local name -> Entity, filled in as pass 1 below creates each entity. A linear scan
		// over a LocalArray, not a HashMap - actor entity counts are small, so this needs no
		// allocation, and it stays index-aligned with desc->entities for pass 2.
		struct LocalNameEntry
		{
			Id64 nameId;
			Entity entity;
		};
		LocalArray<LocalNameEntry, c_MaxActorTypeEntities> nameToEntity;

		// Pass 1: create every entity and add its declared components, in declaration order.
		for (const ActorEntityDesc& entityDesc : desc->entities)
		{
			const Entity entity = entities.CreateEntity();

			LocalNameEntry& entry = nameToEntity.ExpandOne();
			entry.nameId = entityDesc.localNameId;
			entry.entity = entity;

			result.Add(entity);

			for (const ActorComponentDesc& componentDesc : entityDesc.components)
			{
				componentDesc.addFn(entities, entity, componentDesc.data);
			}
		}

		// Pass 2: every local name now maps to a real Entity, so parent links resolve
		// correctly no matter which order they were declared in.
		for (uint i = 0; i < desc->entities.Size(); ++i)
		{
			const ActorEntityDesc& entityDesc = desc->entities[i];
			if (entityDesc.parentLocalNameId == c_InvalidActorLocalName)
			{
				continue;
			}

			Entity parentEntity = c_InvalidEntity;
			for (const LocalNameEntry& entry : nameToEntity)
			{
				if (entry.nameId == entityDesc.parentLocalNameId)
				{
					parentEntity = entry.entity;
					break;
				}
			}
			// A parent name with no matching entity in this same actor definition means a bad
			// AddEntity(..., parentLocalName) call somewhere in this actor type.
			TYR_ASSERT(parentEntity != c_InvalidEntity);

			// Requires this entity to already have a ComponentTransform from its own
			// AddComponent calls - an entity that declares a parent but no ComponentTransform
			// is a malformed actor definition.
			ComponentTransform& transform = entities.GetComponent<ComponentTransform>(nameToEntity[i].entity);
			transform.parentEntity = parentEntity;
		}

		return result;
	}
}
