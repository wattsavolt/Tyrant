#pragma once

#include "RenderBase/RenderHandles.h"

namespace tyr
{
	struct UploadBufferAllocation 
	{
		RenderBufferHandle buffer;
		void* cpuPtr;
		size_t offset;
		size_t size;
	};

	struct GpuBufferAllocation
	{
		RenderBufferHandle buffer;
		size_t offset;
		size_t size;
	};
}