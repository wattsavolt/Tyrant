#pragma once

#include "CoreMacros.h"

#if defined(TYR_RENDERER_EXPORTS)
#		define TYR_RENDERER_EXPORT TYR_EXPORT
#else
#		define TYR_RENDERER_EXPORT TYR_IMPORT
#endif

// Later configure this to be disabled on consoles in the final build
#define TYR_SHADER_COMPILATION_ENABLED 1

