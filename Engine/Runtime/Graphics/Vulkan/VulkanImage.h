#pragma once

#include "RenderAPI/Image.h"
#include "VulkanDevice.h"

namespace tyr
{
	class VulkanImage final : public Image
	{
	public:
		VulkanImage(VulkanDevice& device, const ImageDesc& desc);
		~VulkanImage();

		VkImage GetImage() const { return m_Image; }

	private:
		VulkanDevice& m_Device;	
		VkImage m_Image;
		VmaAllocation m_Allocation;
	};

	class VulkanImageView final : public ImageView
	{
	public:
		VulkanImageView(VulkanDevice& device, const ImageViewDesc& desc);
		~VulkanImageView();

		VkImageView GetImageView() const { return m_ImageView; }

	private:
		VulkanDevice& m_Device;
		VkImageView m_ImageView;
	};

	class VulkanSampler final : public Sampler
	{
	public:
		VulkanSampler(VulkanDevice& device, const SamplerDesc& desc);
		~VulkanSampler();

		VkSampler GetSampler() const { return m_Sampler; }

	private:
		VulkanDevice& m_Device;
		VkSampler m_Sampler;
	};
}
