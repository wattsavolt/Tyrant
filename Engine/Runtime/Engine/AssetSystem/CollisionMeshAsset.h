#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "Math/Vector3.h"
#include "IO/BufferedFileStream.h"

namespace tyr
{
	class PhysicsShape;

	// A mesh's collision, made by the importer from its UCX_ meshes or its own triangles.
	// Only lives on the CPU, loaded while a Mesh or ConvexHull collider uses it.
	struct CollisionMesh
	{
		// Written first and bumped whenever the layout changes. The high half is a tag.
		static constexpr uint c_FileVersion = 0x54434C01;

		// For static and kinematic bodies. Each holds a reference, and either can be null.
		const PhysicsShape* triangleShape = nullptr;
		// For any body, including dynamic ones.
		const PhysicsShape* convexShape = nullptr;
#if TYR_EDITOR
		// Each shape's edges as pairs of points in mesh space, for drawing.
		Array<Vector3> triangleLines;
		Array<Vector3> convexLines;
#endif

		// Releases the shapes.
		void Reset();
	};

	template<> void Deserialize(BufferedFileStream& stream, CollisionMesh& collisionMesh);
}
