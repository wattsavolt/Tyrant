#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "Math/Vector3.h"
#include "ECS/EntitySystem.h"

namespace tyr
{
	struct World;
	class NavmeshComponentsManager;
	class PhysicsComponentsManager;

	// Walks entities with a NavAgentComponent along the world's navmesh.
	namespace NavAgents
	{
		// Sends the agent walking to the target. A new target clears arrived and stuck.
		TYR_ENGINE_API void SetTarget(World& world, Entity agent, const Vector3& target);

		// Stops the agent where it is.
		TYR_ENGINE_API void Stop(World& world, Entity agent);

		// Finds and follows each agent's path, setting its character's velocity for physics to move
		// it by, and turning it to face the way it goes.
		void Update(World& world, const NavmeshComponentsManager& navmesh, const PhysicsComponentsManager& physics, float deltaTime);
	}
}
