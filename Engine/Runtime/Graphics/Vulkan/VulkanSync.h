#pragma once

#include "RenderAPI/Sync.h"
#include "VulkanCommon.h"

namespace tyr
{
	struct Fence
	{
		VkFence fence;
	};

	struct Semaphore
	{
		VkSemaphore semaphore;
		SemaphoreType type;
	};

	struct Event
	{
		VkEvent event;
	};
}
