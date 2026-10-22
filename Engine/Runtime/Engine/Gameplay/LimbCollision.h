#pragma once

#include "EngineMacros.h"
#include "Core.h"

namespace tyr
{
	struct World;
	class PhysicsComponentsManager;
	class AnimationSystem;

	// Keeps skeletal meshes' limbs out of walls by pushing their characters away from what they sink into.
	namespace LimbCollision
	{
		// Adds each push to its CharacterComponent's moveVelocity, so call it after whatever sets that
		// and before physics moves it, and works out how far each arm swings out of the way.
		void Update(World& world, const PhysicsComponentsManager& physics, AnimationSystem& animation, float deltaTime);
	}
}
