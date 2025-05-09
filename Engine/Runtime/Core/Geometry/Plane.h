

#pragma once

#include "Base/Base.h"
#include "Math/Vector3.h"

namespace tyr
{
	class Ray;
	class Matrix4;

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
		
		/// Returns true if intersects and calculates distance from ray to the plane 
		bool Intersects(const Ray& ray, float& distance) const;

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

		void SetNormal(const Vector3& normal) { m_Normal = normal; }

		const Vector3& GetNormal() const { return m_Normal; }

		void SetDistance(float distance) { m_Distance = distance; }

		float GetDistance() const { return m_Distance; }

	private:
		Vector3 m_Normal;
		float m_Distance;
	};

}
