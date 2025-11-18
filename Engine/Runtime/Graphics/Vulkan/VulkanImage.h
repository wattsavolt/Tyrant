#pragma once

#include "RenderAPI/Image.h"
#include "VulkanCommon.h"

namespace tyr
{
	struct Image
	{
		VkImage image;
		VmaAllocation allocation;
		VkFormat format;
		VkImageUsageFlags usage;
		VkMemoryPropertyFlagBits memoryProperty;
		bool isExternal = false;
	};

	struct ImageView
	{
		VkImageView imageView;
		ImageHandle image;
	};

	struct Sampler
	{
		VkSampler sampler;
	};
}
