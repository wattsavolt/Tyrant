#pragma once

#include "EngineMacros.h"
#include "Reflection/Reflection.h"
#include "Actor/ActorRegistry.h"

namespace tyr
{
	// Registers a compile-time actor type, the same way TYR_COMPONENT_START/END registers a
	// component - a static-storage instance whose constructor runs before main() and calls
	// ActorRegistry::Instance().RegisterActorType(...). Everything between TYR_ACTOR_START and
	// TYR_ACTOR_END is ordinary C++ using actorTypeDesc.AddEntity(...).AddComponent(...) - no
	// per-entity or per-component macros.
	#define TYR_ACTOR_START(type) \
		struct type##ActorMetaClass \
		{ \
			type##ActorMetaClass() \
			{ \
				ActorTypeDesc& actorTypeDesc = ActorRegistry::Instance().RegisterActorType(Id64(#type), #type);

	#define TYR_ACTOR_END() \
			} \
		} TYR_REFL_CONCAT(actorMetaClassInst_, __LINE__);
}
