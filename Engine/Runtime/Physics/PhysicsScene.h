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

		// How far, and which way across the ground, a capsule from a to b has to move to come out of
		// every body it's sunk into: those that never move, and those that do if asked, apart from the two
		// ignored. Contacts pushing mostly up or down, such as with the floor, are left out.
		Vector3 FindCapsulePush(const Vector3& a, const Vector3& b, float radius, bool includeMoving = false,
			uint ignoreBody = PhysicsConstants::c_InvalidBodyID, uint ignoreBody2 = PhysicsConstants::c_InvalidBodyID) const;

		// PhysicsConstants::c_InvalidCharacter when the scene already has as many as it can move.
		uint AddCharacter(const CharacterDesc& desc);
		void RemoveCharacter(uint character);
		// Moves the character over the time, walking up steps, sticking to floors, riding what
		// it stands on and falling otherwise.
		CharacterState MoveCharacter(uint character, const CharacterMove& move, float deltaTime);
		// Puts the character straight at the position, stopped.
		void SetCharacterPosition(uint character, const Vector3& position);

	private:
		PhysicsContext& m_Context;
		PhysicsSceneData* m_Data = nullptr;
	};
}
