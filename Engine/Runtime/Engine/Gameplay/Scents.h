#pragma once

#include "EngineMacros.h"
#include "Core.h"

namespace tyr
{
	struct World;

	namespace Scents
	{
		// Leaves a marker behind each entity with a ScentComponent every so far it moves, and ages
		// the world's markers, dropping the ones past their lifetime.
		void Update(World& world, float deltaTime);
	}
}
