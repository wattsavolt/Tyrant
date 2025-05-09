
#pragma once

#include "Base/Base.h"

namespace tyr
{
	class Vector2;

	/// Class providing some 2D and 3D geometry utility functions.
	class TYR_CORE_EXPORT GeometryUtil
	{
	public:
		/// Checks if all the points are clockwise or counter-clockwise to a line in 2D.
		static bool PointsOnSideOfLine(const Vector2& p1, const Vector2& p2, const Vector2* points, uint numPoints, bool clockwise);

		/// Checks if two 2D convex shapes intersect using the separating axis theorem. 
		static bool ConvexShapesIntersect(const Vector2* pointsA, uint numPointsA, const Vector2* normalsA, uint numNormalsA,
			const Vector2* pointsB, uint numPointsB, const Vector2* normalsB, uint numNormalsB);

		/// Gets the min and max projection of the points on an axis in 2D.
		static void GetMinMaxProjections(const Vector2* points, uint numPoints, const Vector2& axis, float& minProj, float& maxProj);

		/// Checks if a hexagon and triangle intersect using the separating axis theorem. 
		static bool HexagonIntersectsTriangle(const Vector2& hexCentre, float hexRadius, const Vector2& triV1,
			const Vector2& triV2, const Vector2& triV3);
	};

}
