#pragma once

#include "Core.h"
#include "Math/Vector3.h"
#include "ECS/EntitySystem.h"

namespace tyr
{
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
		static constexpr uint c_MaxBodies = 16384;
		static constexpr uint c_MaxBodyPairs = 65536;
		static constexpr uint c_MaxContactConstraints = 16384;
		static constexpr uint c_InvalidBodyID = 0xFFFFFFFF;
		static constexpr float c_Gravity = -9.81f;
	};

	struct RaycastHit
	{
		Entity entity = c_InvalidEntity;
		Vector3 position;
		Vector3 normal;
		float distance = 0.0f;
	};
}
