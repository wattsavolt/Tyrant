#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "Actor/ActorTypes.h"

namespace tyr
{
	struct World;

	// Trigger volumes, set off by the player walking into them.
	namespace Triggers
	{
		// Sets off each trigger volume the player is in, and lets go of those they've left that don't stay set off.
		void Update(World& world);

		// Whether the trigger volume actor of that name has been set off. False when there isn't one.
		TYR_ENGINE_API bool IsTriggered(const World& world, const ActorName& name);
	}
}
