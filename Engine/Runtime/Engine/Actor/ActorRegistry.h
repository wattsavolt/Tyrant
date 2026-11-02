#pragma once

#include "Core.h"
#include "EngineMacros.h"
#include "Containers/HashMap.h"
#include "Actor/ActorTypes.h"

namespace tyr
{
	class TYR_ENGINE_API ActorRegistry final
	{
	public:
		static ActorRegistry& Instance();

		// Called once, by TYR_ACTOR_END()'s static-init constructor.
		ActorTypeDesc& RegisterActorType(const Id64& typeId, const char* name);

		const ActorTypeDesc* FindActorType(const Id64& typeId) const;

		// Every registered type, such as for the editor's list of actor types.
		const HashMap<Id64, ActorTypeDesc>& GetAll() const { return m_ActorTypes; }

		// Builds one instance of actorTypeId's entities into entities, wiring up
		// TransformComponent::parentEntity per the definition's local-name parent links.
		LocalArray<Entity, c_MaxActorTypeEntities> InstantiateActor(const Id64& actorTypeId, EntitySystem& entities) const;

	private:
		friend ActorEntityDesc& AddActorEntityDesc(ActorTypeDesc& actorType, const char* localName, const char* parentLocalName);
		friend void AddActorComponentDesc(ActorEntityDesc& entity, const Id64& typeID, ActorAddComponentFn addFn, const void* value,
			uint size, uint alignment);

		ActorRegistry() = default;
		~ActorRegistry() = default;

		HashMap<Id64, ActorTypeDesc> m_ActorTypes;
		// Every type's entities, components and default values, each type's one after another so
		// making an actor reads straight through. Types are only ever added, so these never move.
		ActorEntityDesc m_Entities[c_MaxActorDescEntities];
		ActorComponentDesc m_Components[c_MaxActorDescComponents];
		alignas(16) uint8 m_Defaults[c_ActorDefaultsSize];
		uint m_EntityCount = 0;
		uint m_ComponentCount = 0;
		uint m_DefaultsUsed = 0;
		// The type being registered, which entities can only be added to.
		const ActorTypeDesc* m_Registering = nullptr;
	};
}
