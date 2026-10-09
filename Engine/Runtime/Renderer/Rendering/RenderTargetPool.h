#pragma once

#include "Core.h"
#include "RenderBase/RenderHandles.h"

namespace tyr
{
	class Device;
	class RenderRegistry;
	struct TextureDesc;

	// Keeps render targets that are no longer needed, such as a viewport's after it's resized, and
	// hands them back out for a matching request instead of creating a new one. Free ones are let
	// go after a while idle, or oldest first when GPU memory nears its budget. Main thread only.
	class RenderTargetPool final
	{
	public:
		RenderTargetPool(RenderRegistry& registry, Device& device);

		// A free render target matching desc, or a new one. outCreated says which.
		TextureHandle Acquire(const TextureDesc& desc, bool& outCreated);

		// Takes back a render target once the GPU has finished with it.
		void Release(TextureHandle handle, uint64 frameNumber);

		// Lets go of free render targets that have been idle too long, or that are taking memory
		// while GPU memory is near its budget.
		void Trim(uint64 frameNumber);

		// Deletes every free render target, such as at shutdown.
		void Clear();

	private:
		struct FreeTarget
		{
			TextureHandle handle;
			uint64 releasedFrame;
		};

		// Kept a few seconds, since a viewport going back to an earlier size usually does so soon.
		static constexpr uint64 c_MaxIdleFrames = 300;
		// The share of the GPU memory budget beyond which free render targets are let go.
		static constexpr float c_BudgetThreshold = 0.85f;

		RenderRegistry& m_Registry;
		Device& m_Device;
		// Oldest release first.
		Array<FreeTarget> m_FreeTargets;
	};
}
