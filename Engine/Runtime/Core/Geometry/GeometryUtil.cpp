

#include "GeometryUtil.h"
#include "Math/Vector2.h"

namespace tyr
{
	bool GeometryUtil::PointsOnSideOfLine(const Vector2& p1, const Vector2& p2, const Vector2* points, uint numPoints, bool clockwise)
	{
		for (uint i = 0; i < numPoints; ++i)
		{
			const Vector2& p3 = points[i];

			// Outer Product: d = (x - x1)(y2 - y1) - (y - y1)(x2 - x1)
			float d = ((p3.x - p1.x) * (p2.y - p1.y)) - ((p3.y - p1.y) * (p2.x - p1.x));

			if (clockwise)
			{
				if (d <= 0)
				{
					return false;
				}
			}
			else
			{
				if (d >= 0)
				{
					return false;
				}
			}
		}

		return true;
	}

    bool GeometryUtil::ConvexShapesIntersect(const Vector2* pointsA, uint numPointsA, const Vector2* normalsA, uint numNormalsA,
        const Vector2* pointsB, uint numPointsB, const Vector2* normalsB, uint numNormalsB)
    {
        // Test with first shape's normals.
        for (uint i = 0; i < numNormalsA; ++i)
        {
            float minProjA;
            float maxProjA;
            float minProjB;
            float maxProjB;

            GetMinMaxProjections(pointsA, numPointsA, normalsA[i], minProjA, maxProjA);
            GetMinMaxProjections(pointsB, numPointsB, normalsA[i], minProjB, maxProjB);

            if (maxProjA < minProjB || maxProjB < minProjA)
            {
                return false;
            }
        }

        // Test with second shape's normals.
        for (uint i = 0; i < numNormalsB; ++i)
        {
            float minProjA;
            float maxProjA;
            float minProjB;
            float maxProjB;

            GetMinMaxProjections(pointsA, numPointsA, normalsB[i], minProjA, maxProjA);
            GetMinMaxProjections(pointsB, numPointsB, normalsB[i], minProjB, maxProjB);

            if (maxProjA < minProjB || maxProjB < minProjA)
            {
                return false;
            }
        }

        return true;
    }

    void GeometryUtil::GetMinMaxProjections(const Vector2* points, uint numPoints, const Vector2& axis, float& minProj, float& maxProj)
    {
        minProj = FLT_MAX;
        maxProj = -FLT_MAX;
        for (uint i = 0; i < numPoints; ++i)
        {
            float proj = points[i].Dot(axis);
            if (proj < minProj)
            {
                minProj = proj;
            }
            if (proj > maxProj)
            {
                maxProj = proj;
            }
        }
    }

    bool GeometryUtil::HexagonIntersectsTriangle(const Vector2& hexCentre, float hexRadius, const Vector2& triV1,
        const Vector2& triV2, const Vector2& triV3)
    {
        // Hexagon points
        const Vector2 right = Vector2(hexRadius, 0.0f);
        const Vector2 left = -right;
        const Vector2 topRight = Vector2::FromRadiusAndAngle(hexRadius, Math::c_DegToRad * 60.0f);
        const Vector2 topLeft = Vector2::FromRadiusAndAngle(hexRadius, Math::c_DegToRad * 120.0f);
        const Vector2 bottomRight = -topLeft;
        const Vector2 bottomLeft = -topRight;
        const Vector2 hexPoints[6] = { bottomLeft, left, topLeft, topRight, right, bottomRight };

        // Hexagon normals. Three only needed. One per axis.
        const Vector2 leftDiagNorm = Vector2::SafeNormalize(topLeft - left).PerpCCW();
        const Vector2 topNorm = Vector2::SafeNormalize(topRight - topLeft).PerpCCW();
        const Vector2 rightDiagNorm = Vector2::SafeNormalize((right - topRight).PerpCCW());
        const Vector2 hexNormals[3] = { leftDiagNorm, topNorm, rightDiagNorm };

        // Triangle points
        const Vector2 triPoints[3] = { triV1, triV2, triV3 };

        // Triangle normals. One per edge.
        const Vector2 triNorm1 = Vector2::SafeNormalize(triV2 - triV1).PerpCCW();
        const Vector2 triNorm2 = Vector2::SafeNormalize(triV3 - triV2).PerpCCW();
        const Vector2 triNorm3 = Vector2::SafeNormalize((triV1 - triV3).PerpCCW());
        const Vector2 triNormals[3] = { triNorm1, triNorm2, triNorm3 };

        return ConvexShapesIntersect(hexPoints, 6, hexNormals, 3, triPoints, 3, triNormals, 3);
    }
}
