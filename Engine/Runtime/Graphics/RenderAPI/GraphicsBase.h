#pragma once

#include "Core.h"
#include "GraphicsMacros.h"

namespace tyr
{
	#define TYR_GRAPHICS_ASSERT(r) \
			if (r != 0) \
			{ \
				TYR_LOG_ERROR("Render API Result Code: %d", static_cast<int64>(r)); \
				TYR_ASSERT(false); \
			}
	#define TYR_GASSERT(r) TYR_GRAPHICS_ASSERT(r)	

	/// Max number of GPU queues per type. 
	#define TYR_MAX_QUEUES_PER_TYPE 8

	enum class RenderAPIBackend : uint8
	{
		Vulkan,
		D3D12
	};
}