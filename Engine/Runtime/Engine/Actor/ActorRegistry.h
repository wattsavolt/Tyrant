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

		// For the future editor dropdown - nothing consumes this yet.
		const HashMap<Id64, ActorTypeDesc>& GetAll() const { return m_ActorTypes; }

		// Builds one instance of actorTypeId's entities into entities, wiring up
		// ComponentTransform::parentEntity per the definition's local-name parent links.
		LocalArray<Entity, c_MaxActorTypeEntities> InstantiateActor(const Id64& actorTypeId, EntitySystem& entities) const;

	private:
		ActorRegistry() = default;
		~ActorRegistry() = default;

		HashMap<Id64, ActorTypeDesc> m_ActorTypes;
	};
}
