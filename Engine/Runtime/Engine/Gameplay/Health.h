#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "ECS/EntitySystem.h"

namespace tyr
{
	struct World;

	namespace Health
	{
		// Takes damage off the entity's health, down to 0. Returns true when that killed it.
		TYR_ENGINE_API bool ApplyDamage(World& world, Entity entity, float amount);

		// Puts the entity back to full health, such as when the player starts again.
		TYR_ENGINE_API void Restore(World& world, Entity entity);

		// True once the entity's health has run out. Something without health never dies.
		TYR_ENGINE_API bool IsDead(const World& world, Entity entity);
	}
}
