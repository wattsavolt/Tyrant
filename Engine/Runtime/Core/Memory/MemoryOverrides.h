#pragma once

#include "Memory/Allocation.h"

#if TYR_NEW_OVERRIDE_ENABLED
inline void* TYR_CDECL operator new(size_t cb)
{
	return tyr::MemAlloc(cb);
}

inline void* TYR_CDECL operator new[](size_t cb)
{
	return tyr::MemAlloc(cb);
}

inline void* TYR_CDECL operator new(size_t cb, const std::nothrow_t&) noexcept
{
	return tyr::MemAlloc(cb);
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

