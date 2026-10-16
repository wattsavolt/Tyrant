#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "Math/Vector3.h"

namespace tyr
{
	struct World;
	class PhysicsComponentsManager;

	// How lit places are, for gameplay such as creatures that shun light.
	namespace LightLevel
	{
		// How brightly the world's lights light the position, in their intensity units, falling off
		// the way they're drawn. Walls between a light and the position block it.
		TYR_ENGINE_API float Get(World& world, const PhysicsComponentsManager& physics, const Vector3& position);

		// True when a held flashlight that's on shines on the position: within its beam's range and
		// angle, with nothing in the way.
		TYR_ENGINE_API bool IsInFlashlightBeam(World& world, const PhysicsComponentsManager& physics, const Vector3& position);
	}
}
