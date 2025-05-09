#include "Plane.h"
#include "Ray.h"
#include "Math/Vector4.h"
#include "Math/Matrix4.h"

namespace tyr
{
	Plane::Plane(const Vector3& normal, float d)
		: m_Normal(normal), m_Distance(d)
	{ }

	Plane::Plane(float a, float b, float c, float d)
		: m_Normal(a, b, c), m_Distance(d)
	{ }

	Plane::Plane(const Vector3& normal, const Vector3& point)
		: m_Normal(normal), m_Distance(normal.Dot(point))
	{ }

	Plane::Plane(const Vector3& point0, const Vector3& point1, const Vector3& point2)
	{
		Vector3 edge0 = point1 - point0;
		Vector3 edge1 = point2 - point0;
		m_Normal = edge0.Cross(edge1);
		m_Normal.Normalize();
		m_Distance = m_Normal.Dot(point0);
	}

	float Plane::GetDistanceFromPoint(const Vector3& point) const
	{
		return m_Normal.Dot(point) - m_Distance;
	}
	
	bool Plane::Intersects(const Ray& ray, float& distance) const
	{
		float denom = m_Normal.Dot(ray.GetDirection());
		if (Math::Abs(denom) < std::numeric_limits<float>::epsilon())
		{
			// Parallel
			distance = 0.0f;
			return false;
		}
		else
		{
			// Below is cheaper but nom could also be calculated by taking the ray's origin from the centre point on the plane
			// and getting the dot product of that and the plane's normal. Then distance = nom / denom
			float nom = m_Normal.Dot(ray.GetOrigin()) - m_Distance;
			distance = -(nom / denom);
			return distance >= 0.0f;
		}
	}

	Plane Plane::MultiplyByAffineMatrix(const Matrix4& mat) const
	{
		Vector4 localNormal(m_Normal.x, m_Normal.y, m_Normal.z, 0.0f);
		Vector4 localPoint = localNormal * m_Distance;
		localPoint.w = 1.0f;

		Matrix4 itMat = mat.Inverse().Transpose();
		Vector4 worldNormal = itMat.MultiplyAffine(localNormal);
		Vector4 worldPoint = mat.MultiplyAffine(localPoint);

		float d = worldNormal.Dot(worldPoint);

		return Plane(worldNormal.x, worldNormal.y, worldNormal.z, d);
	}
}
