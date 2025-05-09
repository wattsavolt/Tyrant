#include "Math/Vector3I.h"

namespace tyr
{
	const Vector3I Vector3I::c_Zero(ArgZero::Zero);
	const Vector3I Vector3I::c_One(1, 1, 1);
	const Vector3I Vector3I::c_Infinity(Math::c_Infinity, Math::c_Infinity, Math::c_Infinity);

	const Vector3I Vector3I::c_Right(1, 0, 0);
	const Vector3I Vector3I::c_Up(0, 1, 0);
	const Vector3I Vector3I::c_Forward(0, 0, 1);
}
