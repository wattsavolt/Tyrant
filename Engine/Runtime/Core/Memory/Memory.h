#pragma once

#include "MemoryTypes.h"
#include "Allocation.h"
#include "MemoryOverrides.h"
#include "StackAllocation.h"
#include "Memory/LocalObjectPool.h"
#include "Memory/ObjectPool.h"

namespace tyr
{
	#ifndef TYR_SAFE_RELEASE
	#define TYR_SAFE_RELEASE(p)  { if(p) { p->Release(); p = nullptr; } }
	#endif

	template <typename T>
	inline constexpr auto RefCast = [](const Ref<void>& ptr) -> Ref<T> 
	{
		return std::static_pointer_cast<T>(ptr);
	};
}

