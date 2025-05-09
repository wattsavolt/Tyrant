
#include "VulkanSync.h"

namespace tyr
{
	VulkanFence::VulkanFence(VulkanDevice& device, const FenceDesc& desc)
		: m_Device(device)
		, Fence(desc)
	{
		VkFenceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		createInfo.pNext = nullptr;
		createInfo.flags = desc.signalled ? VK_FENCE_CREATE_SIGNALED_BIT : 0; 

		TYR_GASSERT(vkCreateFence(device.GetLogicalDevice(), &createInfo, g_VulkanAllocationCallbacks, &m_Fence));
		VulkanUtility::SetDebugName(device.GetLogicalDevice(), desc.debugName.c_str(), VK_OBJECT_TYPE_FENCE, reinterpret_cast<uint64>(m_Fence));
	}

	VulkanFence::~VulkanFence()
	{
		vkDestroyFence(m_Device.GetLogicalDevice(), m_Fence, g_VulkanAllocationCallbacks);	
	}

	void VulkanFence::Reset()
	{
		TYR_GASSERT(vkResetFences(m_Device.GetLogicalDevice(), 1, &m_Fence));
	}

	bool VulkanFence::GetStatus() const
	{
		VkResult result = vkGetFenceStatus(m_Device.GetLogicalDevice(), m_Fence);
		if (result == VK_SUCCESS)
		{
			return true;
		}
		else if (result == VK_NOT_READY)
		{
			return false;
		}
		TYR_ASSERT(false);
		return false;
	}

	bool VulkanFence::Wait()
	{
		VkResult result = vkWaitForFences(m_Device.GetLogicalDevice(), 1, &m_Fence, VK_TRUE, m_Desc.timeout);
		if (result == VK_SUCCESS)
		{
			return true;
		}
		else if (result == VK_TIMEOUT)
		{
			return false;
		}
		TYR_ASSERT(false);
		return false;
	}

	VulkanSemaphore::VulkanSemaphore(VulkanDevice& device, const SemaphoreDesc& desc)
		: m_Device(device)
		, Semaphore(desc)
	{
		VkSemaphoreCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		createInfo.pNext = desc.type == SemaphoreType::Timeline ? &createInfo : nullptr;
		createInfo.flags = 0;

		if (desc.type == SemaphoreType::Timeline)
		{
			VkSemaphoreTypeCreateInfo typeCreateInfo;
			typeCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
			typeCreateInfo.pNext = nullptr;
			typeCreateInfo.initialValue = 0;
			typeCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
			createInfo.pNext = &typeCreateInfo;
		}

		TYR_GASSERT(vkCreateSemaphore(device.GetLogicalDevice(), &createInfo, g_VulkanAllocationCallbacks, &m_Semaphore));
		VulkanUtility::SetDebugName(device.GetLogicalDevice(), desc.debugName.c_str(), VK_OBJECT_TYPE_SEMAPHORE, reinterpret_cast<uint64>(m_Semaphore));
	}

	VulkanSemaphore::~VulkanSemaphore()
	{
		vkDestroySemaphore(m_Device.GetLogicalDevice(), m_Semaphore, g_VulkanAllocationCallbacks);
	}

	void VulkanSemaphore::Signal(uint64 value)
	{
		TYR_ASSERT(m_Desc.type == SemaphoreType::Timeline);
		VkSemaphoreSignalInfo info;
		info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
		info.pNext = nullptr;
		info.semaphore = m_Semaphore;
		info.value = value;
		TYR_GASSERT(vkSignalSemaphore(m_Device.GetLogicalDevice(), &info));
	}

	uint64 VulkanSemaphore::GetValue() const
	{
		TYR_ASSERT(m_Desc.type == SemaphoreType::Timeline);
		uint64 value;
		TYR_GASSERT(vkGetSemaphoreCounterValue(m_Device.GetLogicalDevice(), m_Semaphore, &value));
		return value;
	}

	bool VulkanSemaphore::Wait(uint64 value, uint64 timeout)
	{
		TYR_ASSERT(m_Desc.type == SemaphoreType::Timeline);
		VkSemaphoreWaitInfo info;
		info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
		info.pNext = nullptr;
		info.flags = 0;
		info.semaphoreCount = 1;
		info.pSemaphores = &m_Semaphore;
		info.pValues = &value;

		VkResult result = vkWaitSemaphores(m_Device.GetLogicalDevice(), &info, timeout);
		if (result == VK_SUCCESS)
		{
			return true;
		}
		else if (result == VK_TIMEOUT)
		{
			return false;
		}
		TYR_ASSERT(false);
		return false;
	}

	VulkanEvent::VulkanEvent(VulkanDevice& device, const EventDesc& desc)
		: m_Device(device)
		, Event(desc)
	{
		VkEventCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
		createInfo.pNext = nullptr;
		createInfo.flags = 0;

		TYR_GASSERT(vkCreateEvent(device.GetLogicalDevice(), &createInfo, g_VulkanAllocationCallbacks, &m_Event));
		VulkanUtility::SetDebugName(device.GetLogicalDevice(), desc.debugName.c_str(), VK_OBJECT_TYPE_EVENT, reinterpret_cast<uint64>(m_Event));
	}

	VulkanEvent::~VulkanEvent()
	{
		vkDestroyEvent(m_Device.GetLogicalDevice(), m_Event, g_VulkanAllocationCallbacks);
	}

	bool VulkanEvent::Set()
	{
		TYR_GASSERT(vkSetEvent(m_Device.GetLogicalDevice(), m_Event));
	}

	void VulkanEvent::Reset()
	{
		TYR_GASSERT(vkResetEvent(m_Device.GetLogicalDevice(), m_Event));
	}

	bool VulkanEvent::IsSet() const
	{
		VkResult result = vkGetEventStatus(m_Device.GetLogicalDevice(), m_Event);
		if (result == VK_EVENT_SET)
		{
			return true;
		}
		else if (result == VK_EVENT_RESET)
		{
			return false;
		}
		TYR_GASSERT(result);
		return false;
	}
	
}