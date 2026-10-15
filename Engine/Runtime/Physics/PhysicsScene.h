#pragma once

#include "PhysicsMacros.h"
#include "PhysicsTypes.h"

namespace tyr
{
	class PhysicsContext;
	struct PhysicsSceneData;

	// A world of bodies and characters that collide with each other. Bodies and characters are
	// named by the IDs that adding them returns.
	class TYR_PHYSICS_API PhysicsScene final : INonCopyable
	{
	public:
		explicit PhysicsScene(PhysicsContext& context);
		// Removes every body still in it.
		~PhysicsScene();

		// The body holds its own reference to the shape. PhysicsConstants::c_InvalidBodyID when out of bodies.
		uint AddBody(const BodyDesc& desc);
		void RemoveBody(uint body);
		// Keeps queries fast after adding a lot of bodies at once, such as on level load.
		void OptimizeBroadPhase();

		// Moves a kinematic body to the transform over the time, pushing what's in the way.
		void MoveKinematic(uint body, const Vector3& position, const Quaternion& rotation, float deltaTime);
		void GetBodyTransform(uint body, Vector3& outPosition, Quaternion& outRotation) const;

		// Simulates the bodies over the time. Characters move separately, with MoveCharacter.
		void Step(float deltaTime);

		// The closest body hit along the ray, within maxDistance.
		bool Raycast(const Vector3& origin, const Vector3& direction, float maxDistance, PhysicsRayHit& outHit) const;

		// PhysicsConstants::c_InvalidCharacter when the scene already has as many as it can move.
		uint AddCharacter(const CharacterDesc& desc);
		void RemoveCharacter(uint character);
		// Moves the character over the time, walking up steps, sticking to floors, riding what
		// it stands on and falling otherwise.
		CharacterState MoveCharacter(uint character, const CharacterMove& move, float deltaTime);

	private:
		PhysicsContext& m_Context;
		PhysicsSceneData* m_Data = nullptr;
	};
}
