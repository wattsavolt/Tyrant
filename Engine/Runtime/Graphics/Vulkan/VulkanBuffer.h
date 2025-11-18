#pragma once

#include "RenderAPI/Buffer.h"
#include "VulkanCommon.h"

namespace tyr
{
	struct Buffer
	{
		VkBuffer buffer;
		VmaAllocation allocation;
		VkDeviceSize size;
		uint stride;
		VkBufferUsageFlags usage;
		VkMemoryPropertyFlagBits memoryProperty;
	};

	struct BufferView
	{
		VkBufferView bufferView;
		BufferHandle buffer;
		size_t offset;
		size_t size;
	};
}
