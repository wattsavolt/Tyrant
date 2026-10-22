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

		// Stops the agent where it is, cutting short any dash.
		TYR_ENGINE_API void Stop(World& world, Entity agent);

		// Sends the agent straight along the velocity for so many seconds, ignoring its path and how
		// fast it can speed up, facing the way it goes, such as for a leap. It stops once it's done.
		TYR_ENGINE_API void Dash(World& world, Entity agent, const Vector3& velocity, float seconds);

		// Turns the agent to face the point at its turn speed while it stands still, until it's
		// given a new target.
		TYR_ENGINE_API void FaceTowards(World& world, Entity agent, const Vector3& point);

		// Finds and follows each agent's path, setting its character's velocity for physics to move
		// it by, and turning it to face the way it goes.
		void Update(World& world, const NavmeshComponentsManager& navmesh, const PhysicsComponentsManager& physics, float deltaTime);
	}
}
