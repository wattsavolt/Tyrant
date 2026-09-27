#pragma once

#include "RenderConstants.h"
#include "RenderAPI/Sync.h"
#include "RenderAPI/SwapChain.h"
#include "RenderBase/RenderHandles.h"

namespace tyr
{
	class Device;
	class SwapChain;

	struct RenderWindowFrame
	{
		// Needed before AcquireNextImage returns an image index at all, so this has to be
		// indexed by frame-in-flight (renderFrameIndex), not by image - see
		// RenderWindow::executeCompleteSemaphores for the semaphore that does need the latter.
		SemaphoreHandle aquireSwapChainImageSemaphore;
	};

	struct RenderWindow
	{
		SwapChain* swapChain = nullptr;
		uint swapChainImageIndex = 0;
		RenderWindowFrame frames[RenderConstants::c_BufferedFrameCount]{};
		// Signalled by the submission that renders into a given swap chain image and waited on
		// by that image's Present() call, so - unlike the acquire semaphore above - this one
		// has to be indexed by the acquired swap chain image index, not by frame-in-flight:
		// frame-in-flight and image index aren't guaranteed to cycle in lockstep (the driver
		// doesn't promise AcquireNextImage returns images in a fixed rotation), so indexing
		// this by renderFrameIndex could reuse a given semaphore for a *different* image than
		// the one whose previous Present() actually retired it.
		SemaphoreHandle executeCompleteSemaphores[SwapChain::c_MaxImages]{};
	};

	// A RemoveWindow request queued for deferred processing (see RemoveWindow's own comment).
	// Carries the pool handle alongside the copied resource data so the pool slot itself can be
	// freed at the same safe point as the GPU resources, instead of immediately - see
	// Renderer::DeleteWindowResources.
	struct PendingWindowDelete
	{
		RenderWindowHandle handle;
		RenderWindow window;
	};
}