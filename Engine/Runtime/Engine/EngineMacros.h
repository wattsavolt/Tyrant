

#pragma once

#include "CoreMacros.h"

#if defined(TYR_ENGINE_EXPORTS)
#		define TYR_ENGINE_API TYR_EXPORT
#else
#		define TYR_ENGINE_API TYR_IMPORT
#endif

