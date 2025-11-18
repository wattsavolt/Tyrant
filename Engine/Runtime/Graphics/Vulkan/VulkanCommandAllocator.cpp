/** Copyright (c) 2019-2021 Aidan Clear */

#include "VulkanCommandAllocator.h"
#include "VulkanUtility.h"
#include "VulkanDevice.h"

namespace tyr
{
	VulkanCommandAllocator::VulkanCommandAllocator(DeviceInternal& device, const CommandAllocatorDesc& desc)
		: CommandAllocator(desc)
		, m_Device(device)
	{
		VkCommandPoolCreateInfo ci;
		ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		ci.pNext = nullptr;
		ci.flags = static_cast<VkCommandPoolCreateFlags>(desc.flags);
		ci.queueFamilyIndex = device.GetQueueFamilyIndex(desc.queueType);

		TYR_GASSERT(vkCreateCommandPool(device.GetLogicalDevice(), &ci, g_VulkanAllocationCallbacks, &m_CommandPool));
		VulkanUtility::SetDebugName(device.GetLogicalDevice(), desc.debugName.CStr(), VK_OBJECT_TYPE_COMMAND_POOL, reinterpret_cast<uint64>(m_CommandPool));
	}

	VulkanCommandAllocator::~VulkanCommandAllocator()
	{
		vkDestroyCommandPool(m_Device.GetLogicalDevice(), m_CommandPool, g_VulkanAllocationCallbacks);
	}

	void VulkanCommandAllocator::Reset() 
	{
		vkResetCommandPool(m_Device.GetLogicalDevice(), m_CommandPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
	}
}