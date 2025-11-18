#pragma once

#include <CoreMacros.h>

#if defined(TYR_GRAPHICS_EXPORTS)
#		define TYR_GRAPHICS_EXPORT TYR_EXPORT
#else
#		define TYR_GRAPHICS_EXPORT TYR_IMPORT
#endif

