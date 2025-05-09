

#pragma once

#include "Base/Base.h"
#include "Math/Vector3.h"

namespace tyr
{
	class Ray;

	/// A bounding sphere represented by a centre and radius 
	class TYR_CORE_EXPORT BoundingSphere
	{
	public:
		BoundingSphere() = default;

		BoundingSphere(const BoundingSphere& copy) = default;

		BoundingSphere(const Vector3& centre, float radius);

		bool Intersects(const BoundingSphere& sphere) const;

		/// Returns true if intersects and calculates distance from ray origin to the sphere. 
		///
		/// @param[in]	ray				    Ray to intersect with the sphere.
		/// @param[in]	discardContained	If true, the intersection will be discarded if ray origin
		///                                 is located within the sphere.
		bool Intersects(const Ray& ray, float& distance, bool discardContained = true) const;

		bool operator==(const BoundingSphere& rhs) const
		{
			return (rhs.m_Radius == m_Radius && rhs.m_Centre == m_Centre);
		}

		bool operator!=(const BoundingSphere& rhs) const
		{
			return (rhs.m_Radius != m_Radius || rhs.m_Centre != m_Centre);
		}

		void SetCentre(const Vector3& centre) { m_Centre = centre; }

		const Vector3& GetCentre() const { return m_Centre; }

		void SetRadius(float radius) { m_Radius = radius; }

		float GetRadius() const { return m_Radius; }

	private:
		Vector3 m_Centre;
		float m_Radius;
	};

}
