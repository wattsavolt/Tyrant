#pragma once

#include "CoreMacros.h"

#if defined(TYR_AUDIO_EXPORTS)
#		define TYR_AUDIO_API TYR_EXPORT
#else
#		define TYR_AUDIO_API TYR_IMPORT
#endif
