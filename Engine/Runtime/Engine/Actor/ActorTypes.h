#pragma once

#include "Core.h"
#include "EngineMacros.h"
#include "Containers/LocalArray.h"
#include "String/Name.h"
#include "Identifiers/Identifiers.h"
#include "ECS/EntitySystem.h"
#include <type_traits>
#include <cstring>

namespace tyr
{
	// The most entities one compile-time actor type can declare, and the most components
	// one of its entities can carry.
	constexpr uint c_MaxActorTypeEntities = 32;
	constexpr uint c_MaxActorEntityComponents = 8;

	// Largest component an actor type can attach via ActorEntityDesc::AddComponent - sized
	// with headroom over today's largest component (MeshComponent). AddComponent's own
	// static_assert turns exceeding this into a compile error, not silent truncation.
	constexpr uint c_MaxActorComponentDataSize = 512;

	// Not c_InvalidEntity - parent resolution happens against local name hashes, before any
	// Entity exists yet (see ActorRegistry::InstantiateActor).
	constexpr Id64 c_InvalidActorLocalName = Id64();

	// One already-constructed, trivially-copyable component value plus a stateless function
	// that copies it onto a freshly created entity via EntitySystem::AddComponent<T>. addFn
	// captures nothing - T is baked in through which AddComponent<T> instantiation is chosen,
	// not through a capture - so a plain function pointer is enough here.
	struct ActorComponentDesc
	{
		void (*addFn)(EntitySystem&, Entity, const void*) = nullptr;
		uint8 data[c_MaxActorComponentDataSize];
	};

	struct ActorEntityDesc
	{
		// e.g. the hash of "Hips" - unique only within one actor type's own definition.
		Id64 localNameId = c_InvalidActorLocalName;
		// c_InvalidActorLocalName means this entity is a root, with no parent.
		Id64 parentLocalNameId = c_InvalidActorLocalName;
		LocalArray<ActorComponentDesc, c_MaxActorEntityComponents> components;

		template<typename T>
		void AddComponent(const T& value)
		{
			static_assert(std::is_trivially_copyable_v<T>, "Actor components must be trivially copyable - see ComponentReflection.h");
			static_assert(sizeof(T) <= c_MaxActorComponentDataSize, "Component too large for ActorComponentDesc::data - bump c_MaxActorComponentDataSize");

			ActorComponentDesc& componentDesc = components.ExpandOne();
			memcpy(componentDesc.data, &value, sizeof(T));
			componentDesc.addFn = [](EntitySystem& entities, Entity entity, const void* data)
			{
				entities.AddComponent<T>(entity, *static_cast<const T*>(data));
			};
		}
	};

	struct ActorTypeDesc
	{
		Id64 typeId;
		// Display name, for the future editor dropdown - not used to key the registry.
		Name name;
		LocalArray<ActorEntityDesc, c_MaxActorTypeEntities> entities;

		ActorEntityDesc& AddEntity(const char* localName, const char* parentLocalName = nullptr)
		{
			ActorEntityDesc& entityDesc = entities.ExpandOne();
			entityDesc.localNameId = Id64(localName);
			entityDesc.parentLocalNameId = parentLocalName ? Id64(parentLocalName) : c_InvalidActorLocalName;
			return entityDesc;
		}
	};
}
