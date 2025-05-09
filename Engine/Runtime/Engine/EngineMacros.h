

#pragma once

#include "CoreMacros.h"

#if defined(TYR_ENGINE_EXPORTS)
#		define TYR_ENGINE_EXPORT TYR_EXPORT
#else
#		define TYR_ENGINE_EXPORT TYR_IMPORT
#endif

