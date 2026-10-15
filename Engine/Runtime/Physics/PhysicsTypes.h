#pragma once

#include "Core.h"
#include "Math/Vector3.h"
#include "Math/Quaternion.h"

namespace tyr
{
	// A collision shape, only ever used through pointers. Shapes are counted: whatever makes or
	// takes a shape releases it once it's done.
	class PhysicsShape;

	// What moves a body. Stored as uint8 in RigidBodyComponent.
	enum class MotionType : uint8
	{
		// Never moves, such as walls and floors.
		Static = 0,
		// Moved by gameplay, pushing dynamic bodies out of the way.
		Kinematic,
		// Moved by the simulation.
		Dynamic
	};

	// Stored as uint8 in ColliderComponent.
	enum class ColliderShape : uint8
	{
		Box = 0,
		Sphere,
		// Upright along the entity's Y axis.
		Capsule,
		// The triangles of the entity's mesh. A dynamic body uses ConvexHull instead,
		// since triangle meshes can't be simulated.
		Mesh,
		// The convex hull of the entity's mesh, or of each of its UCX_ pieces.
		ConvexHull
	};

	struct PhysicsConstants
	{
		static constexpr float c_FixedTimeStep = 1.0f / 60.0f;
		// A slow frame catches up by at most this many steps, so it can't fall further behind.
		static constexpr uint c_MaxStepsPerFrame = 4;
		static constexpr uint c_MaxPhysicsWorlds = 4;
		static constexpr uint c_MaxBodies = 16384;
		static constexpr uint c_MaxBodyPairs = 65536;
		static constexpr uint c_MaxContactConstraints = 16384;
		// Characters each physics world can move at once.
		static constexpr uint c_MaxCharacters = 16;
		static constexpr uint c_InvalidBodyID = 0xFFFFFFFF;
		static constexpr uint c_InvalidCharacter = 0xFFFFFFFF;
		static constexpr float c_Gravity = -9.81f;
	};

	struct BodyDesc
	{
		const PhysicsShape* shape = nullptr;
		Vector3 position;
		Quaternion rotation = Quaternion::c_Identity;
		MotionType motion = MotionType::Static;
		float friction = 0.5f;
		float restitution = 0.0f;
		float linearDamping = 0.05f;
		float angularDamping = 0.05f;
		float gravityFactor = 1.0f;
		// 0 works the mass out from the shape.
		float mass = 0.0f;
		// For a moving triangle mesh, which has no volume to work a mass out from.
		bool massFromBounds = false;
		// Handed back by raycasts, such as the entity the body belongs to.
		uint64 userData = 0;
	};

	// An upright capsule that walks rather than being simulated, standing on its position.
	struct CharacterDesc
	{
		Vector3 position;
		float radius = 0.3f;
		float height = 1.75f;
		float crouchHeight = 1.15f;
		// The steepest slope it can walk up, in degrees.
		float maxSlope = 50.0f;
		uint64 userData = 0;
	};

	struct CharacterMove
	{
		// Horizontal, in metres per second. Gravity is added on top.
		Vector3 moveVelocity;
		// Standing back up waits until there's room overhead.
		bool wantsCrouch = false;
		// The tallest step it walks up, and the furthest it sticks to a floor dropping away.
		float stepHeight = 0.35f;
	};

	struct CharacterState
	{
		Vector3 position;
		bool grounded = false;
		bool crouched = false;
	};

	struct PhysicsRayHit
	{
		uint64 userData = 0;
		Vector3 position;
		Vector3 normal;
		float distance = 0.0f;
	};
}
