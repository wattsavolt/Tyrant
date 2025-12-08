#pragma once

#include "Base/Base.h"
#include "Math/Vector3.h"

namespace tyr
{
	class Matrix4;
	class Ray;
	class BoundingSphere;

	/// A class representing a 3D plane
	class TYR_CORE_EXPORT Plane
	{
	public:
		Plane() = default;

		Plane(const Plane& plane) = default;

		Plane(const Vector3& normal, float d);

		Plane(float a, float b, float c, float d);

		Plane(const Vector3& normal, const Vector3& point);

		Plane(const Vector3& point0, const Vector3& point1, const Vector3& point2);

		float GetDistanceFromPoint(const Vector3& point) const;
		
		/// Returns true if intersects and calculates signed distance from ray to the plane 
		bool Intersects(const Ray& ray, float& distance) const;

		/// Returns true if intersects and calculates signed distance from sphere to the plane 
		bool Intersects(const BoundingSphere& sphere, float& distance) const;

		/// Transform a plane by an affine 4x4 matrix.
		Plane MultiplyByAffineMatrix(const Matrix4& mat) const;

		bool operator==(const Plane& rhs) const
		{
			return (rhs.m_Distance == m_Distance && rhs.m_Normal == m_Normal);
		}

		bool operator!=(const Plane& rhs) const
		{
			return (rhs.m_Distance != m_Distance || rhs.m_Normal != m_Normal);
		}

		Vector3 m_Normal;
		float m_Distance;
	};

}
