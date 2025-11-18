#include "VulkanBuffer.h"
#include "VulkanDevice.h"

namespace tyr
{
	BufferHandle Device::CreateBuffer(const BufferDesc& desc)
	{
		BufferHandle handle;
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		Buffer& buffer = *device.m_BufferPool.Create(handle.id);
		VkBufferCreateInfo bufferCI;
		bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCI.pNext = nullptr;
		bufferCI.flags = 0;
		bufferCI.size = static_cast<VkDeviceSize>(desc.size);
		bufferCI.usage = static_cast<VkBufferUsageFlags>(desc.usage);
		bufferCI.sharingMode = VulkanUtility::ToVulkanSharingMode(desc.sharingMode);
		bufferCI.queueFamilyIndexCount = 0;
		bufferCI.pQueueFamilyIndices = nullptr;
		
		TYR_ASSERT(device.HasMemoryType(desc.memoryProperty));
		TYR_GASSERT(vkCreateBuffer(device.m_LogicalDevice, &bufferCI, g_VulkanAllocationCallbacks, &buffer.buffer));

		TYR_SET_GFX_DEBUG_NAME(device.m_LogicalDevice, desc.debugName.CStr(), VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64>(buffer.buffer));

		buffer.size = bufferCI.size;
		buffer.stride = desc.stride;
		buffer.usage = bufferCI.usage;
		buffer.memoryProperty = static_cast<VkMemoryPropertyFlagBits>(desc.memoryProperty);
		buffer.allocation = device.AllocateMemory(buffer.buffer, buffer.memoryProperty);

		return handle;
	}

	void Device::DeleteBuffer(BufferHandle handle)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		Buffer& buffer = device.GetBuffer(handle);
		vkDestroyBuffer(device.m_LogicalDevice, buffer.buffer, g_VulkanAllocationCallbacks);
		device.FreeMemory(buffer.allocation);
		device.m_BufferPool.Delete(handle.id);
	}

	size_t Device::GetBufferSize(BufferHandle handle) const
	{
		const DeviceInternal& device = static_cast<const DeviceInternal&>(*this);
		const Buffer& buffer = device.GetBuffer(handle);
		return static_cast<size_t>(buffer.size);
	}

	uint8* Device::MapBuffer(BufferHandle handle)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		Buffer& buffer = device.GetBuffer(handle);
		TYR_ASSERT(Utility::HasFlag(buffer.memoryProperty, MEMORY_PROPERTY_HOST_VISIBLE_BIT));
		uint8* data;
		TYR_GASSERT(vmaMapMemory(device.m_Allocator, buffer.allocation, (void**)&data));
		return data;
	}

	void Device::UnmapBuffer(BufferHandle handle)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		Buffer& buffer = device.GetBuffer(handle);
		TYR_ASSERT(Utility::HasFlag(buffer.memoryProperty, MEMORY_PROPERTY_HOST_VISIBLE_BIT));
		vmaUnmapMemory(device.m_Allocator, buffer.allocation);
	}

	void Device::WriteBuffer(BufferHandle handle, const void* data, size_t offset, size_t size)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		Buffer& buffer = device.GetBuffer(handle);
		const bool& hostVisible = Utility::HasFlag(buffer.memoryProperty, MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		TYR_ASSERT(hostVisible);
		const bool& hostCoherent = Utility::HasFlag(buffer.memoryProperty, MEMORY_PROPERTY_HOST_COHERENT_BIT);
		uint8* mappedData = MapBuffer(handle);
		memcpy((void*)&mappedData[offset], data, size);
		if (!hostCoherent)
		{
			vmaFlushAllocation(device.m_Allocator, buffer.allocation, 0, VK_WHOLE_SIZE);
		}
		UnmapBuffer(handle);	
	}

	void Device::ReadBuffer(BufferHandle handle, void* data, size_t offset, size_t size)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		Buffer& buffer = device.GetBuffer(handle);
		const bool& hostVisible = Utility::HasFlag(buffer.memoryProperty, MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		TYR_ASSERT(hostVisible);
		const bool& hostCoherent = Utility::HasFlag(buffer.memoryProperty, MEMORY_PROPERTY_HOST_COHERENT_BIT);
		
		const uint8* mappedData = MapBuffer(handle);
		if (!hostCoherent)
		{
			vmaInvalidateAllocation(device.m_Allocator, buffer.allocation, 0, VK_WHOLE_SIZE);
		}
		memcpy(data, (const void*)&mappedData[offset], size);
		UnmapBuffer(handle);
	}

	BufferViewHandle Device::CreateBufferView(const BufferViewDesc& desc)
	{
		BufferViewHandle handle;
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		BufferView& bufferView = *device.m_BufferViewPool.Create(handle.id);
		Buffer& buffer = device.GetBuffer(desc.buffer);

		VkBufferViewCreateInfo bufferViewCI{};
		bufferViewCI.pNext = nullptr;
		bufferViewCI.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
		bufferViewCI.flags = 0;
		bufferViewCI.buffer = buffer.buffer;
		bufferViewCI.format = VK_FORMAT_R8_UINT;
		bufferViewCI.offset = desc.offset;
		bufferViewCI.range = desc.size;

		// Only makes sense to create a buffer view in vulkan for buffers with uniform texel or storage texel usage. 
		if (Utility::HasFlag(buffer.usage, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT) || Utility::HasFlag(buffer.usage, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
		{
			TYR_GASSERT(vkCreateBufferView(device.m_LogicalDevice, &bufferViewCI, g_VulkanAllocationCallbacks, &bufferView.bufferView));
			TYR_SET_GFX_DEBUG_NAME(device.m_LogicalDevice, desc.debugName.CStr(), VK_OBJECT_TYPE_BUFFER_VIEW, reinterpret_cast<uint64>(bufferView.bufferView));
		}
		
		bufferView.buffer = desc.buffer;
		bufferView.offset = desc.offset;
		bufferView.size = desc.size;

		return handle;
	}

	void Device::DeleteBufferView(BufferViewHandle handle)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		BufferView& bufferView = device.GetBufferView(handle);
		Buffer& buffer = device.GetBuffer(bufferView.buffer);
		if (Utility::HasFlag(buffer.usage, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT) || Utility::HasFlag(buffer.usage, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
		{
			vkDestroyBufferView(device.m_LogicalDevice, bufferView.bufferView, g_VulkanAllocationCallbacks);
		}
		device.m_BufferViewPool.Delete(handle.id);
	}
}