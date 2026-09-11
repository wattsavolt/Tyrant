#include "Math/Vector4.h"
#include "Reflection/Reflection.h"

namespace tyr
{
	TYR_REFL_CLASS_START(Vector4, 0);
		TYR_REFL_FIELD(&Vector4::x, "X", true, true, true);
		TYR_REFL_FIELD(&Vector4::y, "Y", true, true, true);
		TYR_REFL_FIELD(&Vector4::z, "Z", true, true, true);
		TYR_REFL_FIELD(&Vector4::w, "W", true, true, true);
	TYR_REFL_CLASS_END();

	const Vector4 Vector4::c_Zero(ArgZero::Zero);
	const Vector4 Vector4::c_One(1, 1, 1, 1);
	const Vector4 Vector4::c_Infinity(Math::c_Infinity, Math::c_Infinity, Math::c_Infinity, Math::c_Infinity);
}

