
#pragma once

#include "Base/Base.h"
#include "Math/Vector3.h"

namespace tyr
{

	class Plane;
	class BoundingSphere;

	/// A ray represented by a direction and origin in 3D space 
	class TYR_CORE_EXPORT Ray
	{

	public:
		Ray() = default;

		Ray(const Vector3& origin, const Vector3& direction)
			: m_Origin(origin), m_Direction(direction) {}

		/// Returns true if intersects and calculates distance from ray to the plane 
		bool Intersects(const Plane& plane, float& distance) const;

		/// Returns true if intersects and calculates distance from ray origin to the sphere.
		///
		/// @param[in]	sphere	          Sphere to test intersection with the ray.
		/// @param[in]	discardContained  (optional) If true the intersection will be discarded if ray origin 
		///                               is located within the sphere.
		bool Intersects(const BoundingSphere& sphere, float& distance, bool discardContained = true) const;

		void SetOrigin(const Vector3& origin) { m_Origin = origin; }

		const Vector3& GetOrigin() const { return m_Origin; }

		void SetDirection(const Vector3& dir) { m_Direction = dir; }

		const Vector3& GetDirection() const { return m_Direction; }

	private:
		Vector3 m_Origin;
		Vector3 m_Direction;
	};

}
