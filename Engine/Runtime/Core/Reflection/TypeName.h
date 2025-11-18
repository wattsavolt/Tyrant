#pragma once

#include "String/LocalString.h"

namespace tyr
{
	// @note template types not accounted for in the size as TypeInfo not created for them
	static constexpr uint c_MaxTypeName = 23;
	using TypeName = LocalString<c_MaxTypeName>;
}