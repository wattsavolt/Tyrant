#pragma once

#include "GraphicsBase.h"
#include "RenderAPITypes.h"
#include "Device.h"

namespace tyr
{
	class Image;
	class ImageView;
	class Semaphore;
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

		virtual uint AcquireNextImage(Ref<Semaphore>& semaphore) = 0;

		// Semaphore should be one used when executing the command list
		virtual void Present(const Ref<CommandList>& commandList, const Ref<Semaphore>& semaphore, uint imageIndex, uint queueIndex = 0u) = 0;

		// Adds barrier needed before rendering to the swap chain image
		void CreateRenderingImageBarrier(ImageBarrier& barrier, uint imageIndex, uint srcQueueFamilyIndex = QUEUE_FAMILY_IGNORED);

		// Adds barrier needed before presenting the swap chain image
		void CreatePresentingImageBarrier(ImageBarrier& barrier, uint imageIndex, uint srcQueueFamilyIndex = QUEUE_FAMILY_IGNORED);

		BarrierAccess GetRenderingWriteAccess() const { return m_RenderingWriteAccess; }

		BarrierAccess GetRenderingReadAccess() const { return m_RenderingReadAccess; }

		ImageLayout GetRenderingLayout() const { return m_RenderingLayout; }

		Device& GetDevice() const { return m_Device; }
		
		const SwapChainDesc& GetDesc() const { return m_Desc; }

		const Array<SRef<Image>>& GetImages() const { return m_Images; }

		const Array<SRef<ImageView>>& GetImageViews() const { return m_ImageViews; }

		const uint GetWidth() const { return m_Width; }

		const uint GetHeight() const { return m_Height; }

	protected:
		void CreateSwapChainImagesAndViews(Handle* imageHandles, uint imageCount);

		Device& m_Device;
		SwapChainDesc m_Desc;
		Array<SRef<Image>> m_Images;
		Array<SRef<ImageView>> m_ImageViews;
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
