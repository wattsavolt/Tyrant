#pragma once

#include <CoreMacros.h>

#if defined(TYR_GRAPHICS_EXPORTS)
#		define TYR_GRAPHICS_API TYR_EXPORT
#else
#		define TYR_GRAPHICS_API TYR_IMPORT
#endif

