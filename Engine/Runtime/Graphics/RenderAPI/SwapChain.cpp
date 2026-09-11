#include "SwapChain.h"
#include "Device.h"

namespace tyr
{
	SwapChain::SwapChain(Device* device)
		: m_Device(device) 
	{
	}

	void SwapChain::CreateSyncData()
	{
		if (m_Desc.createDepth)
		{
			m_RenderingWriteAccess = static_cast<BarrierAccess>(BARRIER_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | BARRIER_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
			m_RenderingReadAccess = static_cast<BarrierAccess>(BARRIER_ACCESS_COLOR_ATTACHMENT_READ_BIT | BARRIER_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT);
			m_RenderingLayout = static_cast<ImageLayout>(IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL | IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
			m_SubresourceAspect = static_cast<SubresourceAspect>(SUBRESOURCE_ASPECT_COLOUR_BIT | SUBRESOURCE_ASPECT_DEPTH_BIT | SUBRESOURCE_ASPECT_STENCIL_BIT);
			m_ImageUsage = static_cast<ImageUsage>(IMAGE_USAGE_COLOUR_ATTACHMENT_BIT | IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
		}
		else
		{
			m_RenderingWriteAccess = BARRIER_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			m_RenderingReadAccess = BARRIER_ACCESS_COLOR_ATTACHMENT_READ_BIT;
			m_RenderingLayout = IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			m_SubresourceAspect = SUBRESOURCE_ASPECT_COLOUR_BIT;
			m_ImageUsage = IMAGE_USAGE_COLOUR_ATTACHMENT_BIT;
		}
	}

	void SwapChain::CreateSwapChainImagesAndViews(SwapChainImageData& imageData, void** imageHandles, uint imageCount)
	{
		for (uint i = 0; i < imageCount; ++i)
		{
			ImageDesc imageDesc;
#if !TYR_FINAL
			const GDebugString imageDebugName = GDebugString("SwapChainImage_") + static_cast<int>(i);
			imageDesc.debugName = imageDebugName.CStr();
#endif
			imageDesc.width = imageData.width;
			imageDesc.height = imageData.height;
			imageDesc.type = ImageType::Image2D;
			imageDesc.externalImage = imageHandles[i];
			imageDesc.format = m_Desc.pixelFormat;
			imageDesc.layout = m_RenderingLayout;
			imageDesc.usage = m_ImageUsage;
			imageData.images.Add(m_Device->CreateImage(imageDesc));

			ImageViewDesc viewDesc;
#if !TYR_FINAL
			const GDebugString viewDebugName = GDebugString("SwapChainImageView_") + static_cast<int>(i);
			viewDesc.debugName = viewDebugName.CStr();
#endif
			viewDesc.isSwapChainView = true;
			viewDesc.image = imageData.images[i];
			viewDesc.viewType = ImageType::Image2D;
			viewDesc.subresourceRange.aspect = SUBRESOURCE_ASPECT_COLOUR_BIT;
			viewDesc.subresourceRange.baseMipLevel = 0;
			viewDesc.subresourceRange.mipCount = 1;
			viewDesc.subresourceRange.baseArrayLayer = 0;
			viewDesc.subresourceRange.arrayLayerCount = 1; // Use 2 for stereoscopic (VR).
			imageData.imageViews.Add(m_Device->CreateImageView(viewDesc));
		}
	}

	void SwapChain::DeleteSwapChainImagesAndViews(SwapChainImageData& imageData)
	{
		for (ImageViewHandle handle : imageData.imageViews)
		{
			m_Device->DeleteImageView(handle);
		}
		imageData.imageViews.Clear();
		for (ImageHandle handle : imageData.images)
		{
			m_Device->DeleteImage(handle);
		}
		imageData.images.Clear();
	}

	void SwapChain::DeleteOtherSwapChainImagesAndViews()
	{
		for (ImageViewHandle handle : m_OldImageData.imageViews)
		{
			m_Device->DeleteImageView(handle);
		}
		m_OldImageData.imageViews.Clear();
		for (ImageHandle handle : m_OldImageData.images)
		{
			m_Device->DeleteImage(handle);
		}
		m_OldImageData.images.Clear();
	}

	void SwapChain::CreateRenderingImageBarrier(ImageBarrier& barrier, ImageHandle imageHandle, uint srcQueueFamilyIndex)
	{
		barrier.srcAccess = BARRIER_ACCESS_NONE;
		barrier.dstAccess = m_RenderingWriteAccess;
		barrier.srcLayout = IMAGE_LAYOUT_UNKNOWN;
		barrier.dstLayout = m_RenderingLayout;
		barrier.image = imageHandle;
		barrier.subresourceRange.aspect = m_SubresourceAspect; 
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.mipCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.arrayLayerCount = 1;
		barrier.srcStage = PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		barrier.dstStage = PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		barrier.srcQueueFamilyIndex = srcQueueFamilyIndex;
	}

	void SwapChain::CreatePresentingImageBarrier(ImageBarrier& barrier, ImageHandle imageHandle, uint srcQueueFamilyIndex)
	{
		barrier.srcAccess = m_RenderingWriteAccess;
		barrier.dstAccess = BARRIER_ACCESS_NONE;
		barrier.srcLayout = m_RenderingLayout;
		barrier.dstLayout = IMAGE_LAYOUT_PRESENT_SRC;
		barrier.image = imageHandle;
		barrier.subresourceRange.aspect = m_SubresourceAspect;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.mipCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.arrayLayerCount = 1;
		barrier.srcStage = PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		barrier.dstStage = PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		barrier.srcQueueFamilyIndex = srcQueueFamilyIndex;
	}
}