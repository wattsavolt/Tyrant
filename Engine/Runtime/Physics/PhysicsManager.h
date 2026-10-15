#pragma once

#include "PhysicsMacros.h"
#include "PhysicsTypes.h"

namespace tyr
{
	class PhysicsContext;
	class PhysicsScene;

	// Owns the physics worlds, each a scene stepped at a fixed rate however long frames take.
	class TYR_PHYSICS_API PhysicsManager final : INonCopyable
	{
	public:
		PhysicsManager();
		~PhysicsManager();

		Handle CreatePhysicsWorld();
		// Removes every body still in it.
		void DestroyPhysicsWorld(Handle physicsWorld);

		PhysicsScene& GetScene(Handle physicsWorld);
		const PhysicsScene& GetScene(Handle physicsWorld) const;

		// Adds the time passed and returns how many fixed steps to run for it. Time it can't catch
		// up on is dropped rather than carried into later frames.
		uint AdvanceTime(Handle physicsWorld, float deltaTime);

		// How far between the last two fixed steps this frame is, from 0 to 1.
		float GetInterpolation(Handle physicsWorld) const;

	private:
		struct PhysicsWorldEntry
		{
			PhysicsScene* scene = nullptr;
			// Time not yet simulated, less than one fixed step.
			float accumulator = 0.0f;
		};

		LocalObjectPool<PhysicsWorldEntry, PhysicsConstants::c_MaxPhysicsWorlds> m_Worlds;
		PhysicsContext* m_Context = nullptr;
	};
}
