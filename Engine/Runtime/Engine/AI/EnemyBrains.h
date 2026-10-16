#pragma once

#include "EngineMacros.h"
#include "Core.h"

namespace tyr
{
	struct World;
	class NavmeshComponentsManager;
	class PhysicsComponentsManager;

	// Decides what each enemy with an EnemyComponent does, from what it senses, and sends its
	// NavAgentComponent where that takes it.
	namespace EnemyBrains
	{
		void Update(World& world, const NavmeshComponentsManager& navmesh, const PhysicsComponentsManager& physics, float deltaTime);

		// The state's name, for showing while debugging.
		TYR_ENGINE_API const char* GetStateName(uint8 state);
	}
}
