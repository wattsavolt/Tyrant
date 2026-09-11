#pragma once

#include "Base/Base.h"

namespace tyr
{
	#ifndef TYR_SAFE_RELEASE
	#define TYR_SAFE_RELEASE(p)  { if(p) { p->Release(); p = nullptr; } }
	#endif

	class TYR_CORE_API MemoryUtil
	{
	public:

		template <typename T>
		static constexpr T Align(T value, T alignment)
		{
			TYR_ASSERT((alignment & (alignment - 1)) == 0); // must be power of two
			return (value + alignment - 1) & ~(alignment - 1);
		}
	};
}

