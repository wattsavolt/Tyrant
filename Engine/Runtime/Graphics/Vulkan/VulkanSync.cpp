#include "VulkanSync.h"
#include "VulkanDevice.h"

namespace tyr
{
	FenceHandle Device::CreateFence(const FenceDesc& desc)
	{
		FenceHandle handle;
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		Fence& fence = *device.m_FencePool.Create(handle.id);
		VkFenceCreateInfo fenceCI = {};
		fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceCI.pNext = nullptr;
		fenceCI.flags = desc.signalled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

		TYR_GASSERT(vkCreateFence(device.m_LogicalDevice, &fenceCI, g_VulkanAllocationCallbacks, &fence.fence));
#if !TYR_FINAL
		VulkanUtility::SetDebugName(device.m_LogicalDevice, desc.debugName.CStr(), VK_OBJECT_TYPE_FENCE, reinterpret_cast<uint64>(fence.fence));
#endif
		return handle;
	}

	void Device::DeleteFence(FenceHandle handle)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		Fence& fence = device.GetFence(handle);
		vkDestroyFence(device.m_LogicalDevice, fence.fence, g_VulkanAllocationCallbacks);
		device.m_FencePool.Delete(handle.id);
	}

	void Device::ResetFence(FenceHandle handle)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		Fence& fence = device.GetFence(handle);
		TYR_GASSERT(vkResetFences(device.m_LogicalDevice, 1, &fence.fence));
	}

	bool Device::GetFenceStatus(FenceHandle handle) const
	{
		const DeviceInternal& device = static_cast<const DeviceInternal&>(*this);
		const Fence& fence = device.GetFence(handle);
		VkResult result = vkGetFenceStatus(device.m_LogicalDevice, fence.fence);
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

	bool Device::WaitForFence(FenceHandle handle, uint64 timeout) const
	{
		const DeviceInternal& device = static_cast<const DeviceInternal&>(*this);
		const Fence& fence = device.GetFence(handle);
		VkResult result = vkWaitForFences(device.m_LogicalDevice, 1, &fence.fence, VK_TRUE, timeout);
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

	SemaphoreHandle Device::CreateSemaphoreResource(const SemaphoreDesc& desc)
	{
		SemaphoreHandle handle;
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		Semaphore& semaphore = *device.m_SemaphorePool.Create(handle.id);

		VkSemaphoreCreateInfo semaphoreCI = {};
		semaphoreCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		semaphoreCI.flags = 0;

		if (desc.type == SemaphoreType::Timeline)
		{
			VkSemaphoreTypeCreateInfo typeCI;
			typeCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
			typeCI.pNext = nullptr;
			typeCI.initialValue = 0;
			typeCI.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
			semaphoreCI.pNext = &typeCI;
		}
		else
		{
			semaphoreCI.pNext = nullptr;
		}

		TYR_GASSERT(vkCreateSemaphore(device.m_LogicalDevice, &semaphoreCI, g_VulkanAllocationCallbacks, &semaphore.semaphore));
#if !TYR_FINAL
		VulkanUtility::SetDebugName(device.m_LogicalDevice, desc.debugName.CStr(), VK_OBJECT_TYPE_SEMAPHORE, reinterpret_cast<uint64>(semaphore.semaphore));
#endif
		semaphore.type = desc.type;

		return handle;
	}

	void Device::DeleteSemaphoreResource(SemaphoreHandle handle)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		Semaphore& semaphore = device.GetSemaphore(handle);
		vkDestroySemaphore(device.m_LogicalDevice, semaphore.semaphore, g_VulkanAllocationCallbacks);
		device.m_SemaphorePool.Delete(handle.id);
	}

	void Device::SignalSemaphore(SemaphoreHandle handle, uint64 value)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		Semaphore& semaphore = device.GetSemaphore(handle);
		TYR_ASSERT(semaphore.type == SemaphoreType::Timeline);
		VkSemaphoreSignalInfo info;
		info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
		info.pNext = nullptr;
		info.semaphore = semaphore.semaphore;
		info.value = value;
		TYR_GASSERT(vkSignalSemaphore(device.m_LogicalDevice, &info));
	}

	uint64 Device::GetSemaphoreValue(SemaphoreHandle handle) const
	{
		const DeviceInternal& device = static_cast<const DeviceInternal&>(*this);
		const Semaphore& semaphore = device.GetSemaphore(handle);
		TYR_ASSERT(semaphore.type == SemaphoreType::Timeline);
		uint64 value;
		TYR_GASSERT(vkGetSemaphoreCounterValue(device.m_LogicalDevice, semaphore.semaphore, &value));
		return value;
	}

	bool Device::WaitForSemaphore(SemaphoreHandle handle, uint64 value, uint64 timeout) const
	{
		const DeviceInternal& device = static_cast<const DeviceInternal&>(*this);
		const Semaphore& semaphore = device.GetSemaphore(handle);
		TYR_ASSERT(semaphore.type == SemaphoreType::Timeline);
		VkSemaphoreWaitInfo info;
		info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
		info.pNext = nullptr;
		info.flags = 0;
		info.semaphoreCount = 1;
		info.pSemaphores = &semaphore.semaphore;
		info.pValues = &value;

		VkResult result = vkWaitSemaphores(device.m_LogicalDevice, &info, timeout);
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

	EventHandle Device::CreateEventResource(const EventDesc& desc)
	{
		EventHandle handle;
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		Event& event = *device.m_EventPool.Create(handle.id);

		VkEventCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
		createInfo.pNext = nullptr;
		createInfo.flags = 0;

		TYR_GASSERT(vkCreateEvent(device.m_LogicalDevice, &createInfo, g_VulkanAllocationCallbacks, &event.event));
#if !TYR_FINAL
		VulkanUtility::SetDebugName(device.m_LogicalDevice, desc.debugName.CStr(), VK_OBJECT_TYPE_EVENT, reinterpret_cast<uint64>(event.event));
#endif
		return handle;
	}

	void Device::DeleteEventResource(EventHandle handle)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		Event& event = device.GetEvent(handle);
		vkDestroyEvent(device.m_LogicalDevice, event.event, g_VulkanAllocationCallbacks);
		device.m_EventPool.Delete(handle.id);
	}

	bool Device::SetEvent(EventHandle handle)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		Event& event = device.GetEvent(handle);
		TYR_GASSERT(vkSetEvent(device.m_LogicalDevice, event.event));
	}

	void Device::ResetEvent(EventHandle handle)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		Event& event = device.GetEvent(handle);
		TYR_GASSERT(vkResetEvent(device.m_LogicalDevice, event.event));
	}

	bool Device::IsEventSet(EventHandle handle) const
	{
		const DeviceInternal& device = static_cast<const DeviceInternal&>(*this);
		const Event& event = device.GetEvent(handle);
		VkResult result = vkGetEventStatus(device.m_LogicalDevice, event.event);
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