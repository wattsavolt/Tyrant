#pragma once

#include "RenderConstants.h"
#include "RenderAPI/Sync.h"

namespace tyr
{
	class Device;
	class CommandQueue;
	class CommandAllocator;
	class CommandList;

	struct FrameContext
	{
		CommandAllocator* commandAllocator;
		Array<CommandList*> commandLists;
	};

	struct RenderContext
	{
		Device* device = nullptr;
		CommandQueue* graphicsQueue = nullptr;
		CommandQueue* computeQueue = nullptr;
		CommandQueue* transferQueue = nullptr;
		FrameContext frameContexts[RenderConstants::c_BufferedFrameCount];
		GraphicsRect renderArea;
		FenceHandle completionFence;
		SemaphoreHandle completionSemaphore;
		// Value must be greater than the initial value 0, which the semaphore was created with 
		uint64 currentTimelineValue = 1;
	};

}