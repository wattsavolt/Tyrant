#pragma once

#include "Base/Base.h"
#include "Math/Vector2.h"
#include "Math/Vector2I.h"

namespace tyr
{
	class TYR_CORE_EXPORT Rectangle
	{
	public:
		Vector2 m_Offset;
		Vector2 m_Extents;
	};
}
