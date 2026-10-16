#pragma once

#include "EngineMacros.h"
#include "Core.h"

namespace tyr
{
	struct World;
	class PhysicsComponentsManager;

	// What enemies with a PerceptionComponent sense of what they hunt.
	namespace Perception
	{
		// Updates what each enemy knows of its target from sight, sound and smell, a few times a
		// second. What it knows ages as time passes without news.
		void Update(World& world, const PhysicsComponentsManager& physics, float deltaTime);
	}
}
