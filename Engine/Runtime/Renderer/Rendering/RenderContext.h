#pragma once

#include "RenderConstants.h"
#include "RenderAPI/RenderAPITypes.h"

namespace tyr
{
	class Device;
	class CommandQueue;
	class CommandAllocator;
	class CommandList;

	struct FrameContext
	{
		// One command allocator/list group per queue type.
		CommandAllocator* commandAllocators[CommandQueueType::CQ_COUNT] = {};
		Array<CommandList*> commandLists[CommandQueueType::CQ_COUNT];
	};

	struct RenderContext
	{
		Device* device = nullptr;
		CommandQueue* graphicsQueue = nullptr;
		CommandQueue* computeQueue = nullptr;
		CommandQueue* transferQueue = nullptr;
		FrameContext frameContexts[RenderConstants::c_BufferedFrameCount];
		GraphicsRect renderArea;
	};

}