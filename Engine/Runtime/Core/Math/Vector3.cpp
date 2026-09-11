#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Reflection/Reflection.h"

namespace tyr
{
	TYR_REFL_CLASS_START(Vector3, 0);
		TYR_REFL_FIELD(&Vector3::x, "X", true, true, true);
		TYR_REFL_FIELD(&Vector3::y, "Y", true, true, true);
		TYR_REFL_FIELD(&Vector3::z, "Z", true, true, true);
	TYR_REFL_CLASS_END();

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

		if (lenProduct < 1e-6f)
		{
			return 0.0f;
		}

		auto d = Dot(dest) / lenProduct;

		d = Math::Clamp(d, -1.0f, 1.0f);
		return Math::Acos(d);
	}
}
