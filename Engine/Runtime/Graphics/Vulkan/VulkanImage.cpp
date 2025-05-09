#include "VulkanImage.h"
#include "VulkanDevice.h"

namespace tyr
{
	VulkanImage::VulkanImage(VulkanDevice& device, const ImageDesc& desc)
		: Image(device, desc)
		, m_Device(device)
	{
		VkImageCreateInfo imageCI;
		imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCI.pNext = nullptr;

		switch (desc.type)
		{
		case ImageType::Image1D:
			imageCI.imageType = VK_IMAGE_TYPE_1D;
			break;
		case ImageType::Image1DArray:
		case ImageType::Image2D:
			imageCI.imageType = VK_IMAGE_TYPE_2D;
			break;
		case ImageType::Cubemap:
		case ImageType::CubemapArray:
			imageCI.imageType = VK_IMAGE_TYPE_2D;
			imageCI.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
			break;
		case ImageType::Image2DArray:
		case ImageType::Image3D:
			imageCI.imageType = VK_IMAGE_TYPE_3D;
			imageCI.flags = VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
			break;
		}
		
		imageCI.format = VulkanUtility::ToVulkanPixelFormat(desc.format);
		imageCI.extent = { desc.width, desc.height, desc.depth };
		imageCI.mipLevels = desc.mipLevelCount;
		imageCI.arrayLayers = desc.arrayLayerCount;
		imageCI.samples = static_cast<VkSampleCountFlagBits>(desc.sampleCount);
		imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCI.usage = static_cast<VkImageUsageFlags>(desc.usage) | (desc.mipLevelCount > 1 ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0);
		imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCI.queueFamilyIndexCount = 0;
		imageCI.pQueueFamilyIndices = nullptr;
		imageCI.initialLayout = static_cast<VkImageLayout>(desc.layout);

		if (desc.externalImage)
		{
			m_Image = reinterpret_cast<VkImage>(desc.externalImage);
			m_Allocation = nullptr;
		}
		else
		{
			TYR_GASSERT(vkCreateImage(device.GetLogicalDevice(), &imageCI, g_VulkanAllocationCallbacks, &m_Image));
			m_Allocation = device.AllocateMemory(m_Image, static_cast<VkMemoryPropertyFlagBits>(m_Desc.memoryProperty), m_SizeAllocated);
		}
#if TYR_DEBUG
		VulkanUtility::SetDebugName(device.GetLogicalDevice(), desc.debugName.c_str(), VK_OBJECT_TYPE_IMAGE, reinterpret_cast<uint64>(m_Image));
#endif
	}
	
	VulkanImage::~VulkanImage()
	{
		// Don't delete external image.
		if (!m_Desc.externalImage)
		{
			vkDestroyImage(m_Device.GetLogicalDevice(), m_Image, g_VulkanAllocationCallbacks);
			m_Device.FreeMemory(m_Allocation);
		}
	}

	VulkanImageView::VulkanImageView(VulkanDevice& device, const ImageViewDesc& desc)
		: ImageView(desc)
		, m_Device(device)
	{
		const Ref<VulkanImage>& image = RefCast<VulkanImage>(desc.image);

		VkImageViewCreateInfo imageViewCI;
		imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCI.pNext = nullptr;
		imageViewCI.flags = 0;
		imageViewCI.image = image->GetImage();
		imageViewCI.format = VulkanUtility::ToVulkanPixelFormat(image->GetDesc().format);
		imageViewCI.viewType = static_cast<VkImageViewType>(desc.viewType);
		imageViewCI.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
		imageViewCI.subresourceRange.aspectMask = static_cast<VkImageAspectFlags>(desc.subresourceRange.aspect);
		imageViewCI.subresourceRange.baseMipLevel = desc.subresourceRange.baseMipLevel;
		imageViewCI.subresourceRange.levelCount = desc.subresourceRange.mipLevelCount;
		imageViewCI.subresourceRange.baseArrayLayer = desc.subresourceRange.baseArrayLayer;
		imageViewCI.subresourceRange.layerCount = desc.subresourceRange.arrayLayerCount;

		vkCreateImageView(device.GetLogicalDevice(), &imageViewCI, g_VulkanAllocationCallbacks, &m_ImageView);
		VulkanUtility::SetDebugName(device.GetLogicalDevice(), desc.debugName.c_str(), VK_OBJECT_TYPE_IMAGE_VIEW, reinterpret_cast<uint64>(m_ImageView));
	}

	VulkanImageView::~VulkanImageView()
	{
		// Swapchain image view will be deleted in the VulkanSwapChain class
		if (!m_Desc.isSwapChainView)
		{
			vkDestroyImageView(m_Device.GetLogicalDevice(), m_ImageView, g_VulkanAllocationCallbacks);
		}
	}

	VulkanSampler::VulkanSampler(VulkanDevice& device, const SamplerDesc& desc)
		: Sampler(desc)
		, m_Device(device)
	{
		VkSamplerCreateInfo samplerCI;
		samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCI.pNext = nullptr;
		samplerCI.flags = 0;
		samplerCI.magFilter = VulkanUtility::ToVulkanFilter(desc.magFilter);
		samplerCI.minFilter = VulkanUtility::ToVulkanFilter(desc.minFilter);
		samplerCI.mipmapMode = VulkanUtility::ToVulkanSamplerMipmapMode(desc.mipmapMode);
		samplerCI.addressModeU = VulkanUtility::ToVulkanSamplerAddressMode(desc.addressModeU);
		samplerCI.addressModeV = VulkanUtility::ToVulkanSamplerAddressMode(desc.addressModeV);
		samplerCI.addressModeW = VulkanUtility::ToVulkanSamplerAddressMode(desc.addressModeW);
		samplerCI.mipLodBias = desc.mipLodBias;
		samplerCI.anisotropyEnable = desc.anisotropyEnable;
		samplerCI.maxAnisotropy = desc.maxAnisotropy;
		samplerCI.compareEnable = desc.compareEnable;
		samplerCI.compareOp = VulkanUtility::ToVulkanCompareOp(desc.compareOp);
		samplerCI.minLod = desc.minLod;
		samplerCI.maxLod = desc.maxLod;
		samplerCI.borderColor = VulkanUtility::ToVulkanBorderColour(desc.borderColour);
		samplerCI.unnormalizedCoordinates = desc.unnormalisedCoords;

		vkCreateSampler(device.GetLogicalDevice(), &samplerCI, g_VulkanAllocationCallbacks, &m_Sampler);
#if TYR_DEBUG
		VulkanUtility::SetDebugName(device.GetLogicalDevice(), desc.debugName.c_str(), VK_OBJECT_TYPE_SAMPLER, reinterpret_cast<uint64>(m_Sampler));
#endif
	}

	VulkanSampler::~VulkanSampler()
	{
		vkDestroySampler(m_Device.GetLogicalDevice(), m_Sampler, g_VulkanAllocationCallbacks);
	}
}