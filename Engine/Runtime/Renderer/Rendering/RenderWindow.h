#pragma once

#include "RenderConstants.h"
#include "RenderAPI/Sync.h"

namespace tyr
{
	class Device;
	class SwapChain;

	struct RenderWindowFrame
	{
		SemaphoreHandle aquireSwapChainImageSemaphore;
		SemaphoreHandle executeCompleteSemaphore;
	};

	struct RenderWindow
	{
		SwapChain* swapChain = nullptr;
		uint swapChainImageIndex = 0;
		// Only used by main thread
		bool resizeRequired = false;
		RenderWindowFrame frames[RenderConstants::c_BufferedFrameCount]{};
	};
}