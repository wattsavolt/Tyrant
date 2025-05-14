#pragma once

#include "CoreMacros.h"
#include "Primitives.h"

#include <filesystem>

// C limits
#include <float.h>

extern "C" {

#include <sys/types.h>
#include <sys/stat.h>
#include <cstdarg>

}

#if TYR_PLATFORM == TYR_PLATFORM_WINDOWS
#  undef min
#  undef max
#	if !defined(NOMINMAX) && defined(_MSC_VER)
#		define NOMINMAX // required to stop windows.h messing up std::min
#	endif
#  if defined( __MINGW32__ )
#    include <unistd.h>
#  endif
#endif

#if TYR_PLATFORM == TYR_PLATFORM_LINUX
extern "C" {

#include <unistd.h>
#include <dlfcn.h>

}
#endif

#if TYR_PLATFORM == TYR_PLATFORM_OSX
extern "C" {

#include <unistd.h>
#include <sys/param.h>
#include <CoreFoundation/CoreFoundation.h>

}
#endif

#if defined(_MSC_VER)
#	define TYR_DEBUG_BREAK __debugbreak()
#else
#	include <signal.h>
#	define TYR_DEBUG_BREAK raise(SIGTRAP)
#endif

// Assert
#if TYR_DEBUG 
#	define TYR_ASSERT(v) { if ( !(v) ) { TYR_DEBUG_BREAK; } }
#else
#	define TYR_ASSERT(v)
#endif

#define TYR_STATIC_ASSERT(v, m) static_assert(v, m)

#define TYR_MAX_PATH 260

namespace tyr
{
    typedef void* Handle;
    typedef Handle FileHandle;
}
