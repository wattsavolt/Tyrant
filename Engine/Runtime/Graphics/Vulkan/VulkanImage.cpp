#include "VulkanImage.h"
#include "VulkanDevice.h"

namespace tyr
{
	ImageHandle Device::CreateImage(const ImageDesc& desc)
	{
		ImageHandle handle;
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		Image& image = *device.m_ImagePool.Create(handle.id);
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

		image.format = imageCI.format;
		image.usage = imageCI.usage;
	
		image.memoryProperty = static_cast<VkMemoryPropertyFlagBits>(desc.memoryProperty);

		if (desc.externalImage)
		{
			image.image = reinterpret_cast<VkImage>(desc.externalImage);
			image.allocation = nullptr;
			image.isExternal = true;
		}
		else
		{
			TYR_GASSERT(vkCreateImage(device.m_LogicalDevice, &imageCI, g_VulkanAllocationCallbacks, &image.image));
			image.allocation = device.AllocateMemory(image.image, image.memoryProperty);
			image.isExternal = false;
		}
		TYR_SET_GFX_DEBUG_NAME(device.m_LogicalDevice, desc.debugName.CStr(), VK_OBJECT_TYPE_IMAGE, reinterpret_cast<uint64>(image.image));
		return handle;
	}
	
	void Device::DeleteImage(ImageHandle handle)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		Image& image = device.GetImage(handle);

		// Don't delete external image.
		if (!image.isExternal)
		{
			vkDestroyImage(device.m_LogicalDevice, image.image, g_VulkanAllocationCallbacks);
			device.FreeMemory(image.allocation);
		}

		device.m_ImagePool.Delete(handle.id);
	}

	ImageViewHandle Device::CreateImageView(const ImageViewDesc& desc)
	{
		ImageViewHandle handle;
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		ImageView& imageView = *device.m_ImageViewPool.Create(handle.id);
		Image& image = device.GetImage(desc.image);

		VkImageViewCreateInfo imageViewCI;
		imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCI.pNext = nullptr;
		imageViewCI.flags = 0;
		imageViewCI.image = image.image;
		imageViewCI.format = image.format;
		imageViewCI.viewType = static_cast<VkImageViewType>(desc.viewType);
		imageViewCI.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
		imageViewCI.subresourceRange.aspectMask = static_cast<VkImageAspectFlags>(desc.subresourceRange.aspect);
		imageViewCI.subresourceRange.baseMipLevel = desc.subresourceRange.baseMipLevel;
		imageViewCI.subresourceRange.levelCount = desc.subresourceRange.mipLevelCount;
		imageViewCI.subresourceRange.baseArrayLayer = desc.subresourceRange.baseArrayLayer;
		imageViewCI.subresourceRange.layerCount = desc.subresourceRange.arrayLayerCount;

		vkCreateImageView(device.m_LogicalDevice, &imageViewCI, g_VulkanAllocationCallbacks, &imageView.imageView);
		TYR_SET_GFX_DEBUG_NAME(device.m_LogicalDevice, desc.debugName.CStr(), VK_OBJECT_TYPE_IMAGE_VIEW, reinterpret_cast<uint64>(imageView.imageView));

		imageView.image = desc.image;

		return handle;
	}

	void Device::DeleteImageView(ImageViewHandle handle)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		ImageView& imageView = device.GetImageView(handle);
		Image& image = device.GetImage(imageView.image);
		vkDestroyImageView(device.m_LogicalDevice, imageView.imageView, g_VulkanAllocationCallbacks);
		device.m_ImageViewPool.Delete(handle.id);
	}

	SamplerHandle Device::CreateSampler(const SamplerDesc& desc)
	{
		SamplerHandle handle;
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		Sampler& sampler = *device.m_SamplerPool.Create(handle.id);

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

		vkCreateSampler(device.m_LogicalDevice, &samplerCI, g_VulkanAllocationCallbacks, &sampler.sampler);
		TYR_SET_GFX_DEBUG_NAME(device.m_LogicalDevice, desc.debugName.CStr(), VK_OBJECT_TYPE_SAMPLER, reinterpret_cast<uint64>(sampler.sampler));
		return handle;
	}

	void Device::DeleteSampler(SamplerHandle handle)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		Sampler& sampler = device.GetSampler(handle);
		vkDestroySampler(device.m_LogicalDevice, sampler.sampler, g_VulkanAllocationCallbacks);
		device.m_SamplerPool.Delete(handle.id);
	}
}