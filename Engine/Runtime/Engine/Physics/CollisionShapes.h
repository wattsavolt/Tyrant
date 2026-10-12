#pragma once

#include "Core.h"
#include "Math/Vector3.h"

namespace JPH
{
	class Shape;
}

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

	// Builds and saves the Jolt shapes a mesh collides with, and loads them back.
	namespace CollisionShapes
	{
		// Writes a triangle mesh of every piece's triangles and a convex hull of each piece.
		// The name is only for warnings, such as a flat mesh having no hull.
		bool Write(BufferedFileStream& stream, const CollisionMeshPiece* pieces, uint pieceCount, const char* meshName);

		// Each shape comes back with a reference held, or null when it's missing or unreadable.
		void Read(BufferedFileStream& stream, const JPH::Shape*& outTriangleShape, const JPH::Shape*& outConvexShape);

		// Drops the reference Read gave. Bodies made from the shape hold their own.
		void Release(const JPH::Shape* shape);

		// Adds three lines per triangle, as pairs of points in the shape's space.
		void AddLines(const JPH::Shape* shape, Array<Vector3>& outLines);
	}
}
