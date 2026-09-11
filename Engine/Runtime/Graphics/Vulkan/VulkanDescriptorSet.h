#pragma once

#include "RenderAPI/DescriptorSet.h"
#include <vulkan/vulkan.h>

namespace tyr
{
	struct DescriptorPool
	{
		VkDescriptorPool pool;
		VkDescriptorPoolCreateFlags flags;
	};

	struct DescriptorSetLayout 
	{
		VkDescriptorSetLayout layout;
		LocalArray<VkDescriptorType, c_MaxDescriptorBindings> bindingDescriptorTypes;
	};

	struct DescriptorSet
	{
		DescriptorPoolHandle pool;
		VkDescriptorSet set;
		DescriptorSetLayoutHandle layout;
	};
}
