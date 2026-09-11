#pragma once

#include "Base/Base.h"

namespace tyr
{
#define TYR_POOL_VALIDATION_ASSERTS_ENABLED 1
#if TYR_POOL_VALIDATION_ASSERTS_ENABLED == 1
#	define TYR_POOL_VALIDATION_ASSERT(v) TYR_ASSERT(v)
#else
#	define TYR_POOL_VALIDATION_ASSERT(v)
#endif
}

