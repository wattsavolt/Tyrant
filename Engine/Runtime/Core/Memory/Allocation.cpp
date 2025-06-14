#pragma once

#include "Memory/Allocation.h"

namespace tyr
{
	uint64 TYR_THREADLOCAL MemoryCounter::s_Allocs = 0;
	uint64 TYR_THREADLOCAL MemoryCounter::s_Frees = 0;
}