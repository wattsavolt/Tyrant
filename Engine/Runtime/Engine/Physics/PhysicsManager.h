#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "PhysicsTypes.h"
#include "Rendering/RenderConstants.h"

namespace JPH
{
	class TempAllocatorImpl;
}

namespace tyr
{
	struct World;
	struct ColliderComponent;
	class AssetManager;
	struct PhysicsWorldData;
	class PhysicsJobSystem;

	// Runs Jolt. Each simulated World gets its own physics world, stepped at a fixed rate.
	class TYR_ENGINE_API PhysicsManager final : INonCopyable
	{
	public:
		static constexpr uint c_MaxPhysicsWorlds = RenderConstants::c_MaxScenes;

		PhysicsManager();
		~PhysicsManager();

		Handle CreatePhysicsWorld();
		// Removes every body still in it.
		void DestroyPhysicsWorld(Handle physicsWorld);

		// Adds bodies for colliders that don't have one, then runs as many fixed steps as the
		// time passed calls for. Moving entities to match is up to the caller.
		// Mesh and convex hull colliders wait for their collision mesh to load.
		void Update(Handle physicsWorld, World& world, const AssetManager& assetManager, float deltaTime);

		// How far between the last two fixed steps this frame is, from 0 to 1.
		float GetInterpolation(Handle physicsWorld) const;

		// The collider gets a new body on the next update, such as after its values changed.
		void RemoveBody(Handle physicsWorld, ColliderComponent& collider);

		// The closest body hit along the ray, within maxDistance.
		bool Raycast(Handle physicsWorld, const Vector3& origin, const Vector3& direction, float maxDistance, RaycastHit& outHit) const;

	private:
		struct PhysicsWorldEntry
		{
			PhysicsWorldData* data = nullptr;
			// Time not yet simulated, less than one fixed step.
			float accumulator = 0.0f;
		};

		void CreateBodies(PhysicsWorldData& data, World& world, const AssetManager& assetManager);
		void Step(PhysicsWorldData& data, World& world);

		LocalObjectPool<PhysicsWorldEntry, c_MaxPhysicsWorlds> m_Worlds;
		PhysicsJobSystem* m_JobSystem = nullptr;
		// Per step scratch memory, allocated once.
		JPH::TempAllocatorImpl* m_TempAllocator = nullptr;
	};
}
