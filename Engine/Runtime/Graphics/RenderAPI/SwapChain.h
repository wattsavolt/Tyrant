#pragma once

#include "GraphicsBase.h"
#include "RenderAPITypes.h"
#include "Device.h"
#include "Image.h"
#include "Sync.h"

namespace tyr
{
	class CommandList;
	struct ImageBarrier;

	struct SwapChainDesc
	{
		bool vSyncEnabled;
		PixelFormat pixelFormat;
		ColorSpace colorSpace;
		bool createDepth = false;
		PixelFormat depthFormat = PF_UNKNOWN;
		// Double-buffering used by default
		bool useTripleBuffering = false;
	};

	/// Class repesenting a swapchain
	class TYR_GRAPHICS_EXPORT SwapChain
	{
	public:
		SwapChain(Device& device, const SwapChainDesc& desc);
		virtual ~SwapChain();

		virtual uint AcquireNextImage(SemaphoreHandle semaphore) = 0;

		// Semaphore should be one used when executing the command list
		virtual void Present(const CommandList* commandList, SemaphoreHandle semaphore, uint imageIndex, uint queueIndex = 0u) = 0;

		// Adds barrier needed before rendering to the swap chain image
		void CreateRenderingImageBarrier(ImageBarrier& barrier, uint imageIndex, uint srcQueueFamilyIndex = QUEUE_FAMILY_IGNORED);

		// Adds barrier needed before presenting the swap chain image
		void CreatePresentingImageBarrier(ImageBarrier& barrier, uint imageIndex, uint srcQueueFamilyIndex = QUEUE_FAMILY_IGNORED);

		BarrierAccess GetRenderingWriteAccess() const { return m_RenderingWriteAccess; }

		BarrierAccess GetRenderingReadAccess() const { return m_RenderingReadAccess; }

		ImageLayout GetRenderingLayout() const { return m_RenderingLayout; }

		Device& GetDevice() const { return m_Device; }
		
		const SwapChainDesc& GetDesc() const { return m_Desc; }

		const LocalArray<ImageHandle, 3>& GetImages() const { return m_Images; }

		const LocalArray<ImageViewHandle, 3>& GetImageViews() const { return m_ImageViews; }

		const uint GetWidth() const { return m_Width; }

		const uint GetHeight() const { return m_Height; }

	protected:
		void CreateSwapChainImagesAndViews(Handle* imageHandles, uint imageCount);
		void DeleteSwapChainImagesAndViews();

		Device& m_Device;
		SwapChainDesc m_Desc;
		LocalArray<ImageHandle, 3> m_Images;
		LocalArray<ImageViewHandle, 3> m_ImageViews;
		BarrierAccess m_RenderingWriteAccess;
		BarrierAccess m_RenderingReadAccess;
		ImageLayout m_RenderingLayout;
		SubresourceAspect m_SubresourceAspect;
		ImageUsage m_ImageUsage;
		uint m_Width;
		uint m_Height;
		bool m_Resized;
	};
}
