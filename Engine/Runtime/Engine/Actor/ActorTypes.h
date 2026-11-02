#pragma once

#include "Core.h"
#include "EngineMacros.h"
#include "Containers/LocalArray.h"
#include "String/Name.h"
#include "String/LocalString.h"
#include "Identifiers/Identifiers.h"
#include "Reflection/ReflectionUtil.h"
#include "ECS/EntitySystem.h"
#include <type_traits>

namespace tyr
{
	// The most entities one actor type can declare, and the most components one of its entities can carry.
	constexpr uint c_MaxActorTypeEntities = 32;
	constexpr uint c_MaxActorEntityComponents = 16;

	// Room for every actor type's defaults together, kept by the ActorRegistry: their entities,
	// their components, and the components' values packed one after another.
	constexpr uint c_MaxActorDescEntities = 256;
	constexpr uint c_MaxActorDescComponents = 1024;
	constexpr uint c_ActorDefaultsSize = 128 * 1024;

	// A placed actor's name, which gameplay can also use to refer to it.
	constexpr uint c_MaxActorName = 63;
	using ActorName = LocalString<c_MaxActorName>;

	// The largest component an actor can carry, so tools can copy any component into a buffer this big.
	constexpr uint c_MaxActorComponentDataSize = 768;

	// Not c_InvalidEntity - parent resolution happens against local name hashes, before any
	// Entity exists yet (see ActorRegistry::InstantiateActor).
	constexpr Id64 c_InvalidActorLocalName = Id64();

	// A run of descriptions that sit one after another in the ActorRegistry's tables.
	template<typename T>
	struct ActorDescRange
	{
		T* first = nullptr;
		uint count = 0;

		uint Size() const { return count; }
		T& operator[](uint index) { return first[index]; }
		const T& operator[](uint index) const { return first[index]; }
		T* begin() { return first; }
		T* end() { return first + count; }
		const T* begin() const { return first; }
		const T* end() const { return first + count; }
	};

	using ActorAddComponentFn = void (*)(EntitySystem&, Entity, const void*);

	// One component's default value and a stateless function that copies it onto a new entity via
	// EntitySystem::AddComponent<T>. T is baked in through which instantiation is chosen, so a plain
	// function pointer is enough.
	struct ActorComponentDesc
	{
		// The component's reflection type ID, so its type is known without adding it.
		Id64 typeID;
		ActorAddComponentFn addFn = nullptr;
		// The value, in the registry's packed defaults.
		const uint8* data = nullptr;
	};

	struct ActorEntityDesc;
	struct ActorTypeDesc;

	// Called by the Add functions below while an actor type registers. Each type's entities, and
	// each entity's components, have to be added one after another.
	TYR_ENGINE_API ActorEntityDesc& AddActorEntityDesc(ActorTypeDesc& actorType, const char* localName, const char* parentLocalName);
	TYR_ENGINE_API void AddActorComponentDesc(ActorEntityDesc& entity, const Id64& typeID, ActorAddComponentFn addFn, const void* value,
		uint size, uint alignment);

	struct ActorEntityDesc
	{
		// e.g. the hash of "Hips" - unique only within one actor type's own definition.
		Id64 localNameId = c_InvalidActorLocalName;
		// c_InvalidActorLocalName means this entity is a root, with no parent.
		Id64 parentLocalNameId = c_InvalidActorLocalName;
		ActorDescRange<ActorComponentDesc> components;

		template<typename T>
		void AddComponent(const T& value)
		{
			static_assert(std::is_trivially_copyable_v<T>, "Actor components must be trivially copyable - see ComponentReflection.h");
			static_assert(sizeof(T) <= c_MaxActorComponentDataSize, "Component too large for an actor - bump c_MaxActorComponentDataSize");
			AddActorComponentDesc(*this, GetTypeID<T>(), [](EntitySystem& entities, Entity entity, const void* data)
			{
				entities.AddComponent<T>(entity, *static_cast<const T*>(data));
			}, &value, sizeof(T), alignof(T));
		}
	};

	struct ActorTypeDesc
	{
		Id64 typeId;
		// Display name, for the editor's list of actor types - not used to key the registry.
		Name name;
		// False for actors only the game spawns, such as the player's character, which the
		// editor doesn't offer for placing.
		bool placeable = true;
		ActorDescRange<ActorEntityDesc> entities;

		ActorEntityDesc& AddEntity(const char* localName, const char* parentLocalName = nullptr)
		{
			return AddActorEntityDesc(*this, localName, parentLocalName);
		}
	};
}
