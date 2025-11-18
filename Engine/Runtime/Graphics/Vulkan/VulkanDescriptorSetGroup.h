#pragma once

#include "RenderAPI/DescriptorSetGroup.h"
#include <vulkan/vulkan.h>

namespace tyr
{
	struct DescriptorPool
	{
		VkDescriptorPool pool;
	};

	struct DescriptorSetLayout 
	{
		VkDescriptorSetLayout layout;
		LocalArray<VkDescriptorType, c_MaxDescriptorBindings> bindingDescriptorTypes;
	};

	struct DescriptorSetGroup
	{
		DescriptorPoolHandle pool;
		LocalArray<VkDescriptorSet, c_MaxDescriptorSetsPerGroup> sets;
		LocalArray<DescriptorSetLayoutHandle, c_MaxDescriptorSetsPerGroup> layouts;
	};
}
