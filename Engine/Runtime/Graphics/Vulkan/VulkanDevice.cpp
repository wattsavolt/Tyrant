#include "VulkanDevice.h"
#include "VulkanHelper.h"
#include "VulkanCommandAllocator.h"
#include "VulkanCommandList.h"

#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

namespace tyr
{
	const Array<const char*> DeviceInternal::c_RequiredDeviceExtensions =
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_MAINTENANCE1_EXTENSION_NAME,
		VK_KHR_MAINTENANCE2_EXTENSION_NAME,
		VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
		//VK_EXT_MESH_SHADER_EXTENSION_NAME,
		//VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION_NAME,
#ifdef TYR_USE_DYNAMIC_RENDERING
		VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
#endif
	};

	DeviceInternal::DeviceInternal(VkInstance instance, VkPhysicalDevice device, uint index)
		: Device(index)
		, m_PhysicalDevice(device)
	{
		// Set to default
		for (uint i = 0; i < c_QueueGroupCount; i++)
		{
			m_QueueGroups[i].familyIndex = (uint)-1;
		}
			
		vkGetPhysicalDeviceProperties(device, &m_VulkanDeviceProperties);
		m_DeviceProperties.maxStorageBufferRange = m_VulkanDeviceProperties.limits.maxStorageBufferRange;
		vkGetPhysicalDeviceFeatures(device, &m_VulkanDeviceFeatures);
		vkGetPhysicalDeviceMemoryProperties(device, &m_VulkanMemoryProperties);

		uint numQueueFamilies;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &numQueueFamilies, nullptr);

		Array<VkQueueFamilyProperties> queueFamilyProperties(numQueueFamilies);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &numQueueFamilies, queueFamilyProperties.Data());

		// Create queues 
		// All values initialized to 0.0
		const float defaultQueuePriorities[c_MaxQueuesPerType] = { };
		LocalArray<VkDeviceQueueCreateInfo, CommandQueueType::CQ_COUNT> queueCreateInfos;

		auto PopulateQueueInfo = [&](CommandQueueType type, uint32_t familyIndex)
		{
			VkDeviceQueueCreateInfo& createInfo = queueCreateInfos.ExpandOne();
			createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			createInfo.pNext = nullptr;
			createInfo.flags = 0;
			createInfo.queueFamilyIndex = familyIndex;
			createInfo.queueCount = std::min(queueFamilyProperties[familyIndex].queueCount, (uint)c_MaxQueuesPerType);
			createInfo.pQueuePriorities = defaultQueuePriorities;
			
			m_QueueGroups[type].familyIndex = familyIndex;
			for (uint i = 0; i < createInfo.queueCount; ++i)
			{
				m_QueueGroups[type].queues.Add(VK_NULL_HANDLE);
			}
		};

		// Look for dedicated compute queues
		for (uint i = 0; i < (uint)queueFamilyProperties.Size(); i++)
		{
			if ((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
			{
				PopulateQueueInfo(CQ_COMPUTE, i);
				break;
			}
		}

		// Look for dedicated transfer / upload queues
		for (uint i = 0; i < (uint)queueFamilyProperties.Size(); i++)
		{
			if ((queueFamilyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
				((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) &&
				((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0))
			{
				PopulateQueueInfo(CQ_TRANSFER, i);
				break;
			}
		}

		// Looks for graphics queues
		for (uint i = 0; i < (uint)queueFamilyProperties.Size(); i++)
		{
			if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				PopulateQueueInfo(CQ_GRAPHICS, i);
				break;
			}
		}

		// Set up extensions
		const char* extensions[20];
		uint numExtensions = static_cast<uint>(c_RequiredDeviceExtensions.Size());
		memcpy(extensions, c_RequiredDeviceExtensions.Data(), numExtensions * sizeof(char*));

		// Enumerate supported extensions
		bool dedicatedAllocExt = false;
		bool getMemReqExt = false;

		uint numAvailableExtensions = 0;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &numAvailableExtensions, nullptr);
		if (numAvailableExtensions > 0)
		{
			Array<VkExtensionProperties> availableExtensions(numAvailableExtensions);
			if (vkEnumerateDeviceExtensionProperties(device, nullptr, &numAvailableExtensions, availableExtensions.Data()) == VK_SUCCESS)
			{
				for (const auto& entry : availableExtensions)
				{
					if (strcmp(entry.extensionName, VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME) == 0)
					{
						extensions[numExtensions++] = VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME;
						dedicatedAllocExt = true;
					}
					else if (strcmp(entry.extensionName, VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME) == 0)
					{
						extensions[numExtensions++] = VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME;
						getMemReqExt = true;
					}
				}
			}
		}

		VkDeviceCreateInfo deviceInfo;
		deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceInfo.pNext = nullptr;
		deviceInfo.flags = 0;
		deviceInfo.queueCreateInfoCount = queueCreateInfos.Size();
		deviceInfo.pQueueCreateInfos = queueCreateInfos.Data();
		deviceInfo.pEnabledFeatures = &m_VulkanDeviceFeatures;
		deviceInfo.enabledExtensionCount = numExtensions;
		deviceInfo.ppEnabledExtensionNames = extensions;
		deviceInfo.enabledLayerCount = 0;
		deviceInfo.ppEnabledLayerNames = nullptr;

		VkPhysicalDeviceVulkan13Features vulkanPhysicalDevice13Features{};
		vulkanPhysicalDevice13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
		vulkanPhysicalDevice13Features.pNext = nullptr;

		VkPhysicalDeviceVulkan12Features vulkanPhysicalDevice12Features{};
		vulkanPhysicalDevice12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		vulkanPhysicalDevice12Features.pNext = &vulkanPhysicalDevice13Features;
		
		VkPhysicalDeviceFeatures2 physDevFeatures;
		physDevFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		physDevFeatures.pNext = &vulkanPhysicalDevice12Features;
		vkGetPhysicalDeviceFeatures2(device, &physDevFeatures);

		if (!vulkanPhysicalDevice12Features.descriptorIndexing)
		{
			TYR_ASSERT(false);
			TYR_LOG_FATAL("Descriptor indexing is not supported by the GPU.");
		}

		if (!vulkanPhysicalDevice13Features.synchronization2)
		{
			TYR_ASSERT(false);
			TYR_LOG_FATAL("Synchronization2 is not supported by the GPU.");
		}
		
#ifdef TYR_USE_DYNAMIC_RENDERING
		if (!vulkanPhysicalDevice13Features.dynamicRendering)
		{
			TYR_ASSERT(false);
			TYR_LOG_FATAL("Dynamic rendering is not supported by the GPU.");
		}
#else
		vulkanPhysicalDeviceFeatures.dynamicRendering = VK_FALSE;
#endif	

		if (!vulkanPhysicalDevice12Features.timelineSemaphore)
		{
			TYR_ASSERT(false);
			TYR_LOG_FATAL("Timeline semaphores are not supported by the GPU.");
		}

		deviceInfo.pNext = &vulkanPhysicalDevice12Features;

		TYR_GASSERT(vkCreateDevice(device, &deviceInfo, g_VulkanAllocationCallbacks, &m_LogicalDevice));

		// Retrieve queues
		for (uint i = 0; i < c_QueueGroupCount; i++)
		{
			uint numQueues = (uint)m_QueueGroups[i].queues.Size();
			for (uint j = 0; j < numQueues; j++)
			{
				//VkQueue queue;
				vkGetDeviceQueue(m_LogicalDevice, m_QueueGroups[i].familyIndex, j, &m_QueueGroups[i].queues[j]);
			}
		}

		// Set up the memory allocator
		VmaAllocatorCreateInfo allocatorCI = {};
		allocatorCI.physicalDevice = device;
		allocatorCI.device = m_LogicalDevice;
		allocatorCI.pAllocationCallbacks = g_VulkanAllocationCallbacks;
		allocatorCI.instance = instance;
		allocatorCI.vulkanApiVersion = c_VulkanAPIVersion;

		if (dedicatedAllocExt && getMemReqExt)
		{
			allocatorCI.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
		}

		TYR_GASSERT(vmaCreateAllocator(&allocatorCI, &m_Allocator));
	}

	DeviceInternal::~DeviceInternal()
	{
		TYR_GASSERT(vkDeviceWaitIdle(m_LogicalDevice));

		vmaDestroyAllocator(m_Allocator);
		vkDestroyDevice(m_LogicalDevice, g_VulkanAllocationCallbacks);
	}

	void Device::WaitIdle() const
	{
		const DeviceInternal& device = static_cast<const DeviceInternal&>(*this);
		VkResult result = vkDeviceWaitIdle(device.GetLogicalDevice());
		TYR_GASSERT(result != VK_SUCCESS);
	}

	VmaAllocation DeviceInternal::AllocateMemory(VkBuffer buffer, VkMemoryPropertyFlags flags)
	{
		VmaAllocationCreateInfo allocCI = {};
		allocCI.requiredFlags = flags;

		VmaAllocationInfo allocInfo;
		VmaAllocation allocation;
		TYR_GASSERT(vmaAllocateMemoryForBuffer(m_Allocator, buffer, &allocCI, &allocation, &allocInfo));

		TYR_GASSERT(vkBindBufferMemory(m_LogicalDevice, buffer, allocInfo.deviceMemory, allocInfo.offset));

		return allocation;
	}

	VmaAllocation DeviceInternal::AllocateMemory(VkImage image, VkMemoryPropertyFlags flags)
	{
		VmaAllocationCreateInfo allocCI = {};
		allocCI.requiredFlags = flags;

		VmaAllocationInfo allocInfo;
		VmaAllocation allocation;
		TYR_GASSERT(vmaAllocateMemoryForImage(m_Allocator, image, &allocCI, &allocation, &allocInfo));

		TYR_GASSERT(vkBindImageMemory(m_LogicalDevice, image, allocInfo.deviceMemory, allocInfo.offset));

		return allocation;
	}

	void DeviceInternal::FreeMemory(VmaAllocation allocation)
	{
		vmaFreeMemory(m_Allocator, allocation);
	}

	void DeviceInternal::GetAllocationInfo(VmaAllocation allocation, VkDeviceMemory& memory, VkDeviceSize& offset)
	{
		VmaAllocationInfo allocInfo;
		vmaGetAllocationInfo(m_Allocator, allocation, &allocInfo);

		memory = allocInfo.deviceMemory;
		offset = allocInfo.offset;
	}

	uint Device::FindMemoryType(uint requirementBits, MemoryProperty requestedFlags) const
	{
		const VkMemoryPropertyFlags vkRequestedFlags = static_cast<VkMemoryPropertyFlags>(requestedFlags);
		const DeviceInternal& device = static_cast<const DeviceInternal&>(*this);
		for (uint i = 0; i < device.m_VulkanMemoryProperties.memoryTypeCount; i++)
		{
			if (requirementBits & (1 << i))
			{
				if (Utility::HasFlag(device.m_VulkanMemoryProperties.memoryTypes[i].propertyFlags, vkRequestedFlags))
				{
					return i;
				}
			}
		}
		return -1;
	}

	bool Device::HasMemoryType(MemoryProperty requestedFlags) const
	{
		const VkMemoryPropertyFlags vkRequestedFlags = static_cast<VkMemoryPropertyFlags>(requestedFlags);
		const DeviceInternal& device = static_cast<const DeviceInternal&>(*this);
		for (uint i = 0; i < device.m_VulkanMemoryProperties.memoryTypeCount; i++)
		{	
			if (Utility::HasFlag(device.m_VulkanMemoryProperties.memoryTypes[i].propertyFlags, vkRequestedFlags))
			{
				return i;
			}
		}
	}

	bool Device::IsDiscreteGPU() const
	{
		const DeviceInternal& device = static_cast<const DeviceInternal&>(*this);
		const VkPhysicalDeviceProperties& devProperties = device.GetDeviceProperties();
		return devProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
	}

	CommandAllocator* Device::CreateCommandAllocator(const CommandAllocatorDesc& desc)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		return new VulkanCommandAllocator(device, desc);
	}

	CommandList* Device::CreateCommandList(const CommandListDesc& desc)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		return new CommandListInternal(device, desc);
	}

	// Needs to be in this file as requires implementation of VmaAllocation to call GetSize()
	// and only one .cpp file can define VMA_IMPLEMENTATION before including <vma/vk_mem_alloc.h>
	size_t Device::GetBufferAllocationSize(BufferHandle handle) const
	{
		const DeviceInternal& device = static_cast<const DeviceInternal&>(*this);
		const Buffer& buffer = device.GetBuffer(handle);
		return static_cast<size_t>(buffer.allocation->GetSize());
	}

	// In this file for same reason as above function
	size_t Device::GetImageAllocationSize(ImageHandle handle)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		Image& image = device.GetImage(handle);
		return static_cast<size_t>(image.allocation->GetSize());
	}
}