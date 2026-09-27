#pragma once

#include "RenderBase/RenderHandles.h"
#include "Memory/PoolHandle.h"

namespace tyr
{
	struct UploadBufferAllocation
	{
		RenderBufferHandle buffer;
		void* cpuPtr;
		size_t offset;
		size_t size;
		// Only set for resource upload allocations - pass to
		// RenderAllocationManager::SignalResourceUpload once this allocation's data is
		// actually submitted. Unused for frame upload allocations.
		Handle resourceId;
	};

	struct GpuBufferAllocation
	{
		RenderBufferHandle buffer;
		size_t offset;
		size_t size;
	};
}