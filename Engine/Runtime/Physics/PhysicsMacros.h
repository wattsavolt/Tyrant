#pragma once

#include "CoreMacros.h"

#if defined(TYR_PHYSICS_EXPORTS)
#		define TYR_PHYSICS_API TYR_EXPORT
#else
#		define TYR_PHYSICS_API TYR_IMPORT
#endif
