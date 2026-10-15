#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "PhysicsComponentData.h"

namespace tyr
{
	struct World;
	struct ColliderComponent;
	struct CharacterComponent;
	class AssetManager;
	class PhysicsManager;
	class PhysicsScene;
	class PhysicsShape;
	enum class MotionType : uint8;

	// Keeps a World's physics in step with its components: a body for every collider, a walking
	// character for every CharacterComponent, and dynamic bodies' transforms back on their entities.
	class TYR_ENGINE_API PhysicsComponentsManager final : INonCopyable
	{
	public:
		PhysicsComponentsManager();

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
		// The same for a character, which is made again where its entity is.
		void RemoveCharacter(Handle physicsWorld, CharacterComponent& character);

		// The closest collider hit along the ray, within maxDistance.
		bool Raycast(Handle physicsWorld, const Vector3& origin, const Vector3& direction, float maxDistance, RayCastEntityHit& outHit) const;

		// The shape a collider makes on an entity of the scale, with a reference the caller releases.
		// Null when it has no size yet, or its collision mesh hasn't loaded.
		static const PhysicsShape* CreateColliderShape(const ColliderComponent& collider, const Vector3& scale, MotionType motionType, const AssetManager& assetManager);

	private:
		void CreateBodies(PhysicsScene& scene, World& world, const AssetManager& assetManager);
		void CreateCharacters(PhysicsScene& scene, World& world);
		void Step(PhysicsScene& scene, World& world);
		void StepCharacters(PhysicsScene& scene, World& world);

		PhysicsManager* m_PhysicsManager = nullptr;
	};
}
