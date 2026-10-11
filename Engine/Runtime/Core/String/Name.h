#pragma once

#include "Base/base.h"
#include "LocalString.h"

namespace tyr
{
	class NameConstants
	{
	public:
		static constexpr uint c_MaxName = 31;
		// Include null-terminated character
		static constexpr uint c_MaxNameTotalSize = c_MaxName + 1;
	};

	using Name = LocalString<NameConstants::c_MaxName>;
}