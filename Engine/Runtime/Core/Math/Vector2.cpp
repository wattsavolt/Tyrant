#include "Vector2.h"
#include "Reflection/Reflection.h"

namespace tyr
{
	TYR_REFL_CLASS_START(Vector2, 0);
		TYR_REFL_FIELD(&Vector2::x, "X", true, true, true);
		TYR_REFL_FIELD(&Vector2::y, "Y", true, true, true);
	TYR_REFL_CLASS_END();

	const Vector2 Vector2::c_Zero(ArgZero::Zero);
	const Vector2 Vector2::c_One(1, 1);
	const Vector2 Vector2::c_Right(1, 0);
	const Vector2 Vector2::c_Up(0, 1);
}
