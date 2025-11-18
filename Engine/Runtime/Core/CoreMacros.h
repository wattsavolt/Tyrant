#pragma once

// Initial platform/compiler-related stuff to set.
#define TYR_PLATFORM_WINDOWS 1
#define TYR_PLATFORM_LINUX 2
#define TYR_PLATFORM_OSX 3
#define TYR_PLATFORM_IOS 4
#define TYR_PLATFORM_ANDROID 5

#define TYR_COMPILER_MSVC 1
#define TYR_COMPILER_GNUC 2
#define TYR_COMPILER_INTEL 3
#define TYR_COMPILER_CLANG 4

#define TYR_ARCHITECTURE_x86_32 1
#define TYR_ARCHITECTURE_x86_64 2

#define TYR_ENDIAN_LITTLE 1
#define TYR_ENDIAN_BIG 2
#define TYR_ENDIAN TYR_ENDIAN_LITTLE

// Finds the compiler type and version.
#if defined(__clang__)
#	define TYR_COMPILER TYR_COMPILER_CLANG
#	define TYR_COMP_VER __clang_version__
#	define TYR_THREADLOCAL __thread
#	define TYR_STDCALL __attribute__((stdcall))
#	define TYR_CDECL __attribute__((cdecl))
#	define TYR_FALLTHROUGH [[clang::fallthrough]];
#elif defined(__GNUC__) // Check after Clang, as Clang defines this too
#	define TYR_COMPILER TYR_COMPILER_GNUC
#	define TYR_COMP_VER (((__GNUC__)*100) + \
		(__GNUC_MINOR__*10) + \
		__GNUC_PATCHLEVEL__)
#	define TYR_THREADLOCAL __thread
#	define TYR_STDCALL __attribute__((stdcall))
#	define TYR_CDECL __attribute__((cdecl))
#	define TYR_FALLTHROUGH __attribute__((fallthrough));
#elif defined (__INTEL_COMPILER)
#	define TYR_COMPILER TYR_COMPILER_INTEL
#	define TYR_COMP_VER __INTEL_COMPILER
#	define TYR_STDCALL __stdcall
#	define TYR_CDECL __cdecl
#	define TYR_FALLTHROUGH
	// TYR_THREADLOCAL define is down below because Intel compiler defines it differently based on platform
#elif defined(_MSC_VER) // Check after Clang and Intel, since we could be building with either within VS
#	define TYR_COMPILER TYR_COMPILER_MSVC
#	define TYR_COMP_VER _MSC_VER
#	define TYR_THREADLOCAL __declspec(thread)
#	define TYR_STDCALL __stdcall
#	define TYR_CDECL __cdecl
#	define TYR_FALLTHROUGH
#	undef __PRETTY_FUNCTION__
#	define __PRETTY_FUNCTION__ __FUNCSIG__
#else
#	pragma error "No known compiler. "
#endif

// Finds the current platform
#if defined( __WIN32__ ) || defined( _WIN32 )
#	define TYR_PLATFORM TYR_PLATFORM_WINDOWS
#elif defined( __APPLE_CC__)
#	define TYR_PLATFORM TYR_PLATFORM_OSX
#else
#	define TYR_PLATFORM TYR_PLATFORM_LINUX
#endif

// Find the architecture type
#if defined(__x86_64__) || defined(_M_X64)
#	define TYR_ARCH_TYPE TYR_ARCHITECTURE_x86_64
#else
#	define TYR_ARCH_TYPE TYR_ARCHITECTURE_x86_32
#endif

// DLL export
#if TYR_PLATFORM == TYR_PLATFORM_WINDOWS // Windows
#	if defined(TYR_STATIC_LIB)
#			define TYR_EXPORT
#			define TYR_IMPORT
#	elif TYR_COMPILER == TYR_COMPILER_MSVC
#			define TYR_EXPORT __declspec(dllexport)
#			define TYR_IMPORT __declspec(dllimport)
#	else
#			define TYR_EXPORT __attribute__ ((dllexport))
#			define TYR_IMPORT __attribute__ ((dllimport))
#	endif
#define TYR_HIDDEN
#else // Linux/Mac settings
#	define TYR_EXPORT __attribute__ ((visibility ("default")))
#	define TYR_IMPORT TYR_EXPORT
#	define TYR_HIDDEN __attribute__ ((visibility ("hidden")))
#endif


// Windows Settings
#if TYR_PLATFORM == TYR_PLATFORM_WINDOWS
// Win32 compilers use _DEBUG for specifying debug builds.
// For MinGW, DEBUG is used
#	if defined(_DEBUG) || defined(DEBUG)
#		define TYR_DEBUG 1
#	else
#		define TYR_DEBUG 0
#	endif

#	if TYR_COMPILER == TYR_COMPILER_INTEL
#		define TYR_THREADLOCAL __declspec(thread)
#	endif
#endif

// Linux/Mac Settings
#if TYR_PLATFORM == TYR_PLATFORM_LINUX || TYR_PLATFORM == TYR_PLATFORM_OSX
#	ifdef DEBUG
#		define TYR_DEBUG 1
#	else
#		define TYR_DEBUG 0
#	endif

#	if TYR_COMPILER == TYR_COMPILER_INTEL
#		define TYR_THREADLOCAL __thread
#	endif
#endif

#if defined(TYR_CORE_EXPORTS)
#		define TYR_CORE_EXPORT TYR_EXPORT
#else
#		define TYR_CORE_EXPORT TYR_IMPORT
#endif

// Windows Force Inline
#if TYR_PLATFORM == TYR_PLATFORM_WINDOWS
#  define TYR_FORCEINLINE __forceinline
// Linux/Mac Force Inline
#elif TYR_PLATFORM == TYR_PLATFORM_LINUX || TYR_PLATFORM == TYR_PLATFORM_OSX
#  define TYR_FORCEINLINE inline __attribute__((always_inline))
#else
// Fallback
#  define TYR_FORCEINLINE inline
#endif
