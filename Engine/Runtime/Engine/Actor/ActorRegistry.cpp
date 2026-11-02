#include "ActorRegistry.h"
#include "ECS/Components.h"
#include <cstring>

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
		desc.entities.first = m_Entities + m_EntityCount;
		m_ActorTypes.Insert(typeId, desc);
		ActorTypeDesc* registered = m_ActorTypes.Find(typeId);
		m_Registering = registered;
		return *registered;
	}

	ActorEntityDesc& AddActorEntityDesc(ActorTypeDesc& actorType, const char* localName, const char* parentLocalName)
	{
		ActorRegistry& registry = ActorRegistry::Instance();
		// A type's entities have to follow on from each other, so only the type being registered can add one.
		TYR_ASSERT(&actorType == registry.m_Registering);
		TYR_ASSERT(actorType.entities.first + actorType.entities.count == registry.m_Entities + registry.m_EntityCount);
		TYR_ASSERT(registry.m_EntityCount < c_MaxActorDescEntities && actorType.entities.count < c_MaxActorTypeEntities);

		ActorEntityDesc& entityDesc = registry.m_Entities[registry.m_EntityCount++];
		entityDesc.localNameId = Id64(localName);
		entityDesc.parentLocalNameId = parentLocalName ? Id64(parentLocalName) : c_InvalidActorLocalName;
		entityDesc.components.first = registry.m_Components + registry.m_ComponentCount;
		entityDesc.components.count = 0;
		++actorType.entities.count;
		return entityDesc;
	}

	void AddActorComponentDesc(ActorEntityDesc& entity, const Id64& typeID, ActorAddComponentFn addFn, const void* value,
		uint size, uint alignment)
	{
		ActorRegistry& registry = ActorRegistry::Instance();
		// An entity's components have to follow on from each other, so only the latest entity can take one.
		TYR_ASSERT(entity.components.first + entity.components.count == registry.m_Components + registry.m_ComponentCount);
		TYR_ASSERT(registry.m_ComponentCount < c_MaxActorDescComponents && entity.components.count < c_MaxActorEntityComponents);

		const uint offset = (registry.m_DefaultsUsed + alignment - 1) & ~(alignment - 1);
		TYR_ASSERT(offset + size <= c_ActorDefaultsSize);
		memcpy(registry.m_Defaults + offset, value, size);
		registry.m_DefaultsUsed = offset + size;

		ActorComponentDesc& componentDesc = registry.m_Components[registry.m_ComponentCount++];
		componentDesc.typeID = typeID;
		componentDesc.addFn = addFn;
		componentDesc.data = registry.m_Defaults + offset;
		++entity.components.count;
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

			// Requires this entity to already have a TransformComponent from its own
			// AddComponent calls - an entity that declares a parent but no TransformComponent
			// is a malformed actor definition.
			TransformComponent& transform = entities.GetComponent<TransformComponent>(nameToEntity[i].entity);
			transform.parentEntity = parentEntity;
		}

		return result;
	}
}
