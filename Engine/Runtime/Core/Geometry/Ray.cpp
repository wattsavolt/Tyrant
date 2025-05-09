#include "Ray.h"
#include "Plane.h"
#include "BoundingSphere.h"


namespace tyr
{
	bool Ray::Intersects(const Plane& plane, float& distance) const
	{
		return plane.Intersects(*this, distance);
	}

	bool Ray::Intersects(const BoundingSphere& sphere, float& distance, bool discardContained) const
	{
		return sphere.Intersects(*this, distance);
	}
}
