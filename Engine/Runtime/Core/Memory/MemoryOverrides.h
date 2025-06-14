#pragma once

#include "Memory/Allocation.h"

/// NB: This file should only be included by the Main.cpp file in the application!

#if TYR_NEW_OVERRIDE_ENABLED
inline void* TYR_CDECL operator new(size_t cb)
{
	return tyr::Alloc(cb);
}

inline void* TYR_CDECL operator new[](size_t cb)
{
	return tyr::Alloc(cb);
}

inline void* TYR_CDECL operator new(size_t cb, const std::nothrow_t&) noexcept
{
	return tyr::Alloc(cb);
}

inline void TYR_CDECL operator delete(void* pv)
{
	if (pv)
	{
		tyr::Free(pv);
	}
}

inline void TYR_CDECL operator delete[](void* pv)
{
	if (pv)
	{
		tyr::Free(pv);
	}
}
#endif

