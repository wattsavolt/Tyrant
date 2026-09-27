#pragma once

#include "GraphicsBase.h"
#include "RenderAPITypes.h"
#include "Image.h"
#include "Sync.h"

namespace tyr
{
	class Device;
	class CommandQueue;
	struct ImageBarrier;

	struct SwapChainDesc
	{
		PixelFormat pixelFormat{};
		ColorSpace colorSpace{};
		PixelFormat depthFormat{};
		bool createDepth{};
		bool vSyncEnabled{};
		// Must match however many frames the renderer keeps in flight at once (the caller's
		// buffered-frame count) - a swap chain with fewer images than that will run out of
		// images to acquire before the renderer is done with the ones already acquired.
		uint minImageCount{};
	};

	/// Class repesenting a swapchain
	class TYR_GRAPHICS_API SwapChain
	{
	public:
		static constexpr uint c_MaxImages = 3;

		SwapChain(Device* device);
		virtual ~SwapChain() = default;

		// Recreates for the window specified
		virtual void Recreate(void* windowOSHandle, const SwapChainDesc& desc) = 0;

		// Should only be called when a window resize has occurred. Creates new swapchain, images and image views and switches
		virtual void Resize() = 0;

		virtual void DestroyOldSwapChain() = 0;

		// valid is false when acquisition genuinely failed (VK_ERROR_OUT_OF_DATE_KHR) - the
		// returned index must not be used in that case (the Vulkan spec doesn't guarantee it
		// was written). resizeNeeded is true whenever the swap chain should be recreated
		// (covers both that case and VK_SUBOPTIMAL_KHR, where the index is still usable).
		virtual uint AcquireNextImage(SemaphoreHandle semaphore, bool& valid, bool& resizeNeeded) = 0;

		// Semaphore should be one used when executing the command list
		virtual void Present(const CommandQueue* queue, SemaphoreHandle semaphore, uint imageIndex, bool& resizeNeeded) = 0;

		// Adds barrier needed before rendering to the swap chain image
		void CreateRenderingImageBarrier(ImageBarrier& barrier, ImageHandle imageHandle, uint srcQueueFamilyIndex = QUEUE_FAMILY_IGNORED);

		// Adds barrier needed before presenting the swap chain image
		void CreatePresentingImageBarrier(ImageBarrier& barrier, ImageHandle imageHandle, uint srcQueueFamilyIndex = QUEUE_FAMILY_IGNORED);

		BarrierAccess GetRenderingWriteAccess() const { return m_RenderingWriteAccess; }

		BarrierAccess GetRenderingReadAccess() const { return m_RenderingReadAccess; }

		ImageLayout GetRenderingLayout() const { return m_RenderingLayout; }

		Device* GetDevice() const { return m_Device; }
		
		const SwapChainDesc& GetDesc() const { return m_Desc; }

		const LocalArray<ImageHandle, c_MaxImages>& GetImages() const { return m_ImageData.images; }

		const LocalArray<ImageViewHandle, c_MaxImages>& GetImageViews() const { return m_ImageData.imageViews; }

		const uint GetWidth() const { return m_ImageData.width; }

		const uint GetHeight() const { return m_ImageData.height; }

	protected:
		struct SwapChainImageData
		{
			LocalArray<ImageHandle, c_MaxImages> images;
			LocalArray<ImageViewHandle, c_MaxImages> imageViews;
			uint width{};
			uint height{};
		};

		void CreateSyncData();
		void CreateSwapChainImagesAndViews(SwapChainImageData& imageData, void** imageHandles, uint imageCount);
		void DeleteSwapChainImagesAndViews(SwapChainImageData& imageData);
		void DeleteOtherSwapChainImagesAndViews();

		Device* m_Device;
		SwapChainDesc m_Desc;
		SwapChainImageData m_ImageData{};
		SwapChainImageData m_OldImageData{};
		
		BarrierAccess m_RenderingWriteAccess{};
		BarrierAccess m_RenderingReadAccess{};
		ImageLayout m_RenderingLayout{};
		SubresourceAspect m_SubresourceAspect{};
		ImageUsage m_ImageUsage{};
	};
}
