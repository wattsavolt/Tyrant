#include "VulkanBuffer.h"
#include "VulkanDevice.h"

namespace tyr
{
	VulkanBuffer::VulkanBuffer(VulkanDevice& device, const BufferDesc& desc)
		: Buffer(desc)
		, m_Device(device)
	{
		VkBufferCreateInfo bufferInfo;
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.pNext = nullptr;
		bufferInfo.flags = 0;
		bufferInfo.size = static_cast<VkDeviceSize>(desc.size);
		bufferInfo.usage = static_cast<VkBufferUsageFlags>(desc.usage);
		bufferInfo.sharingMode = VulkanUtility::ToVulkanSharingMode(desc.sharingMode);
		bufferInfo.queueFamilyIndexCount = 0;
		bufferInfo.pQueueFamilyIndices = nullptr;
		
		TYR_ASSERT(device.HasMemoryType(desc.memoryProperty));
		TYR_GASSERT(vkCreateBuffer(device.GetLogicalDevice(), &bufferInfo, g_VulkanAllocationCallbacks, &m_Buffer));

#if TYR_DEBUG
		VulkanUtility::SetDebugName(device.GetLogicalDevice(), desc.debugName.c_str(), VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64>(m_Buffer));
#endif

		m_Allocation = device.AllocateMemory(m_Buffer, static_cast<VkMemoryPropertyFlagBits>(m_Desc.memoryProperty), m_SizeAllocated);
	}

	VulkanBuffer::~VulkanBuffer()
	{
		vkDestroyBuffer(m_Device.GetLogicalDevice(), m_Buffer, g_VulkanAllocationCallbacks);
		m_Device.FreeMemory(m_Allocation);
	}

	uint8* VulkanBuffer::Map() const
	{
		TYR_ASSERT(Utility::HasFlag(m_Desc.memoryProperty, MEMORY_PROPERTY_HOST_VISIBLE_BIT));
		uint8* data;
		TYR_GASSERT(vmaMapMemory(m_Device.GetAllocator(), m_Allocation, (void**)&data));
		return data;
	}

	void VulkanBuffer::Unmap()
	{
		TYR_ASSERT(Utility::HasFlag(m_Desc.memoryProperty, MEMORY_PROPERTY_HOST_VISIBLE_BIT));
		vmaUnmapMemory(m_Device.GetAllocator(), m_Allocation);
	}

	void VulkanBuffer::Write(const void* data, size_t offset, size_t size) 
	{
		const bool& hostVisible = Utility::HasFlag(m_Desc.memoryProperty, MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		TYR_ASSERT(hostVisible);
		const bool& hostCoherent = Utility::HasFlag(m_Desc.memoryProperty, MEMORY_PROPERTY_HOST_COHERENT_BIT); 
		uint8* mappedData = Map();
		memcpy((void*)&mappedData[offset], data, size);
		if (!hostCoherent)
		{
			vmaFlushAllocation(m_Device.GetAllocator(), m_Allocation, 0, VK_WHOLE_SIZE);
		}
		Unmap();	
	}

	void VulkanBuffer::Read(void* data, size_t offset, size_t size)
	{
		const bool& hostVisible = Utility::HasFlag(m_Desc.memoryProperty, MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		TYR_ASSERT(hostVisible);
		const bool& hostCoherent = Utility::HasFlag(m_Desc.memoryProperty, MEMORY_PROPERTY_HOST_COHERENT_BIT);
		
		const uint8* mappedData = Map();
		if (!hostCoherent)
		{
			vmaInvalidateAllocation(m_Device.GetAllocator(), m_Allocation, 0, VK_WHOLE_SIZE);
		}
		memcpy(data, (const void*)&mappedData[offset], size);
		Unmap();
	}

	VulkanBufferView::VulkanBufferView(VulkanDevice& device, const BufferViewDesc& desc)
		: BufferView(desc)
		, m_Device(device)
	{
		const Ref<VulkanBuffer>& buffer = RefCast<VulkanBuffer>(desc.buffer);
		VkBufferViewCreateInfo bufferViewInfo{};
		bufferViewInfo.pNext = nullptr;
		bufferViewInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
		bufferViewInfo.flags = 0;
		bufferViewInfo.buffer = buffer->GetBuffer();
		bufferViewInfo.format = VK_FORMAT_R8_UINT;
		bufferViewInfo.offset = desc.offset;
		bufferViewInfo.range = desc.size;

		const BufferDesc& bufferDesc = desc.buffer->GetDesc();

		// Only makes sense to create a buffer view in vulkan for buffers with uniform texel or storage texel usage. 
		if (Utility::HasFlag(bufferDesc.usage, BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT) || Utility::HasFlag(bufferDesc.usage, BUFFER_USAGE_STORAGE_BUFFER_BIT))
		{
			TYR_GASSERT(vkCreateBufferView(device.GetLogicalDevice(), &bufferViewInfo, g_VulkanAllocationCallbacks, &m_BufferView));
		}
		else
		{
			m_BufferView = nullptr;
		}

		m_Buffer = buffer.get();
	}

	VulkanBufferView::~VulkanBufferView()
	{
		const BufferDesc& bufferDesc = m_Desc.buffer->GetDesc();
		if (Utility::HasFlag(bufferDesc.usage, BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT) || Utility::HasFlag(bufferDesc.usage, BUFFER_USAGE_STORAGE_BUFFER_BIT))
		{
			vkDestroyBufferView(m_Device.GetLogicalDevice(), m_BufferView, g_VulkanAllocationCallbacks);
		}
	}
}