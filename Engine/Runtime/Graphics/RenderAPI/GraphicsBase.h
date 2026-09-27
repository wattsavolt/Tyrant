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

	// A debug name is often a source filename (up to TYR_MAX_FILENAME=63 chars - see Base.h)
	// plus a short suffix (e.g. "_View") - 31 was too tight for that even before any suffix
	// (e.g. "Cube_Material0_AORoughnessMetallic" alone is already 34 chars), which asserted
	// inside LocalString::Set rather than truncating, since a debug name is purely a driver/
	// RenderDoc object label - there's no correctness reason to keep this tight.
	static constexpr uint c_MaxGDebugString = 95;
	static constexpr uint c_MaxGDebugStringTotalSize = c_MaxGDebugString + 1;

	using GDebugString = LocalString<c_MaxGDebugString>;

#if TYR_FINAL
#	define TYR_DECLARE_GDEBUGNAME(name)
#else
#	define TYR_DECLARE_GDEBUGNAME(name) const char* name
#endif

}