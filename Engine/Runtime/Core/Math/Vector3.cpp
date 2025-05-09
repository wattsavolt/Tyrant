

#include "Math/Vector3.h"
#include "Math/Vector4.h"

namespace tyr
{
	const Vector3 Vector3::c_Zero(ArgZero::Zero);
	const Vector3 Vector3::c_One(1, 1, 1);
	const Vector3 Vector3::c_Infinity(Math::c_Infinity, Math::c_Infinity, Math::c_Infinity);

	const Vector3 Vector3::c_Right(1, 0, 0);
	const Vector3 Vector3::c_Up(0, 1, 0);
	const Vector3 Vector3::c_Forward(0, 0, 1);

	Vector3::Vector3(const Vector4& v)
		:x(v.x), y(v.y), z(v.z)
	{

	}

	float Vector3::AngleBetween(const Vector3& dest) const
	{
		float lenProduct = Length() * dest.Length();

		// Divide by zero check
		if (lenProduct < 1e-6f)
		{
			lenProduct = 1e-6f;
		}

		auto d = Dot(dest) / lenProduct;

		d = Math::Clamp(d, -1.0f, 1.0f);
		return Math::Acos(d);
	}
}
