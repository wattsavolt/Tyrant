#pragma once

#include "PhysicsMacros.h"
#include "PhysicsTypes.h"

namespace tyr
{
	class BufferedFileStream;

	// One mesh's triangles, or one convex piece of its authored collision.
	struct CollisionMeshPiece
	{
		const Vector3* positions = nullptr;
		uint vertexCount = 0;
		const uint* indices = nullptr;
		uint indexCount = 0;
	};

	// Makes collision shapes. Each shape returned comes with a reference the caller releases, and
	// is null when it can't be made.
	namespace PhysicsShapes
	{
		// The shapes are moved off the body's origin by offset.
		TYR_PHYSICS_API const PhysicsShape* CreateBox(const Vector3& halfExtents, const Vector3& offset);
		TYR_PHYSICS_API const PhysicsShape* CreateSphere(float radius, const Vector3& offset);
		// Upright along Y. halfHeight is half the straight part, between the two round ends.
		TYR_PHYSICS_API const PhysicsShape* CreateCapsule(float halfHeight, float radius, const Vector3& offset);

		// The shape scaled, sharing its data. Scaling by one gives the shape itself, with another reference.
		TYR_PHYSICS_API const PhysicsShape* CreateScaled(const PhysicsShape* shape, const Vector3& scale);

		TYR_PHYSICS_API void AddRef(const PhysicsShape* shape);
		TYR_PHYSICS_API void Release(const PhysicsShape* shape);

		// Writes a triangle mesh of every piece's triangles and a convex hull of each piece, for
		// ReadMeshShapes. The name is only for warnings, such as a flat mesh having no hull.
		TYR_PHYSICS_API bool WriteMeshShapes(BufferedFileStream& stream, const CollisionMeshPiece* pieces, uint pieceCount, const char* meshName);

		// Each shape comes back with a reference held, or null when it's missing or unreadable.
		TYR_PHYSICS_API void ReadMeshShapes(BufferedFileStream& stream, const PhysicsShape*& outTriangleShape, const PhysicsShape*& outConvexShape);

		// Adds three lines per triangle, as pairs of points in the shape's space.
		TYR_PHYSICS_API void AddLines(const PhysicsShape* shape, Array<Vector3>& outLines);

		// Adds the shape's triangles, three points each, placed in the world at the transform.
		TYR_PHYSICS_API void AddTriangles(const PhysicsShape* shape, const Vector3& position, const Quaternion& rotation, Array<Vector3>& outTriangles);
	}
}
