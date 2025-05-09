#include "BoundingSphere.h"
#include "Ray.h"

namespace tyr
{
	BoundingSphere::BoundingSphere(const Vector3& centre, float radius)
		: m_Centre(centre), m_Radius(radius)
	{ }
	
	bool BoundingSphere::Intersects(const BoundingSphere& sphere) const
	{
		return (sphere.m_Centre - m_Centre).SqrLength() <= Math::Sqr(sphere.m_Radius + m_Radius);
	}

	bool BoundingSphere::Intersects(const Ray& ray, float& distance, bool discardContained) const
	{
		const Vector3& toRayOrig = ray.GetOrigin() - GetCentre();
		float radius = GetRadius();

		// Check origin inside first
		if (toRayOrig.SqrLength() <= radius * radius && discardContained)
		{
			distance = 0.0f;
			return true;
		}

		const Vector3& rayDir = ray.GetDirection();

		// t = (-b +/- sqrt(b*b + 4ac)) / 2a
		float a = rayDir.Dot(rayDir);
		float b = 2 * toRayOrig.Dot(rayDir);
		float c = toRayOrig.Dot(toRayOrig) - radius * radius;

		// Determinant
		float d = (b * b) - (4 * a * c);
		if (d < 0)
		{
			// No intersection
			distance = 0.0f;
			return false;
		}
		else
		{
			// If d == 0 there is one intersection, if d > 0 there are 2.
			// We only return the first one.
			float t = (-b - Math::Sqrt(d)) / (2 * a);
			if (t < 0)
			{
				t = (-b + Math::Sqrt(d)) / (2 * a);
			}
			distance = t;
			return true;
		}
	}
}
