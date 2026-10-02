#include "VulkanDevice.h"
#include "VulkanHelper.h"
#include "VulkanSwapChain.h"
#include "VulkanCommandAllocator.h"
#include "VulkanCommandList.h"
#include "VulkanCommandQueue.h"
#include "Memory/StackAllocation.h"

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
		VK_EXT_MESH_SHADER_EXTENSION_NAME,
		VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION_NAME,
		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
		VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
		VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
		VK_KHR_RAY_QUERY_EXTENSION_NAME,
#ifdef TYR_USE_DYNAMIC_RENDERING
		VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
#endif
	};

	DeviceInternal::DeviceInternal(VkInstance instance, VkPhysicalDevice device, uint index)
		: Device(index)
		, m_Instance(instance)
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

		StackAllocManager stack;
		VkQueueFamilyProperties* queueFamilyProperties = stack.Alloc<VkQueueFamilyProperties>(numQueueFamilies);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &numQueueFamilies, queueFamilyProperties);

		// Create queues 
		// All values initialized to 0.0
		const float defaultQueuePriorities[c_MaxQueuesPerType] = { };
		LocalArray<VkDeviceQueueCreateInfo, CommandQueueType::CQ_COUNT> queueCreateInfos;

		auto PopulateQueueInfo = [&](CommandQueueType type, uint familyIndex)
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
		for (uint i = 0; i < numQueueFamilies; i++)
		{
			if ((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
			{
				PopulateQueueInfo(CQ_COMPUTE, i);
				break;
			}
		}

		// Look for dedicated transfer / upload queues
		for (uint i = 0; i < numQueueFamilies; i++)
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
		for (uint i = 0; i < numQueueFamilies; i++)
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
			const uint availableExtensionCount = numAvailableExtensions;
			VkExtensionProperties* availableExtensions = stack.Alloc<VkExtensionProperties>(availableExtensionCount);
			if (vkEnumerateDeviceExtensionProperties(device, nullptr, &numAvailableExtensions, availableExtensions) == VK_SUCCESS)
			{
				for (uint extIndex = 0; extIndex < availableExtensionCount; ++extIndex)
				{
					const VkExtensionProperties& entry = availableExtensions[extIndex];
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

		VkPhysicalDeviceVulkan11Features vulkanPhysicalDevice11Features{};
		vulkanPhysicalDevice11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
		vulkanPhysicalDevice11Features.pNext = &vulkanPhysicalDevice12Features;

		VkPhysicalDeviceMeshShaderFeaturesEXT vulkanPhysicalDeviceMeshShaderFeatures{};
		vulkanPhysicalDeviceMeshShaderFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
		vulkanPhysicalDeviceMeshShaderFeatures.pNext = &vulkanPhysicalDevice11Features;

		VkPhysicalDeviceUnifiedImageLayoutsFeaturesKHR vulkanPhysicalDeviceUnifiedImageLayoutsFeatures{};
		vulkanPhysicalDeviceUnifiedImageLayoutsFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFIED_IMAGE_LAYOUTS_FEATURES_KHR;
		vulkanPhysicalDeviceUnifiedImageLayoutsFeatures.pNext = &vulkanPhysicalDeviceMeshShaderFeatures;

		VkPhysicalDeviceRayQueryFeaturesKHR vulkanPhysicalDeviceRayQueryFeatures{};
		vulkanPhysicalDeviceRayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
		vulkanPhysicalDeviceRayQueryFeatures.pNext = &vulkanPhysicalDeviceUnifiedImageLayoutsFeatures;

		VkPhysicalDeviceRayTracingPipelineFeaturesKHR vulkanPhysicalDeviceRayTracingPipelineFeatures{};
		vulkanPhysicalDeviceRayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
		vulkanPhysicalDeviceRayTracingPipelineFeatures.pNext = &vulkanPhysicalDeviceRayQueryFeatures;

		VkPhysicalDeviceAccelerationStructureFeaturesKHR vulkanPhysicalDeviceAccelerationStructureFeatures{};
		vulkanPhysicalDeviceAccelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
		vulkanPhysicalDeviceAccelerationStructureFeatures.pNext = &vulkanPhysicalDeviceRayTracingPipelineFeatures;

		VkPhysicalDeviceFeatures2 physDevFeatures;
		physDevFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		physDevFeatures.pNext = &vulkanPhysicalDeviceAccelerationStructureFeatures;
		vkGetPhysicalDeviceFeatures2(device, &physDevFeatures);

		VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties{};
		rayTracingPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
		VkPhysicalDeviceProperties2 deviceProperties2{};
		deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		deviceProperties2.pNext = &rayTracingPipelineProperties;
		vkGetPhysicalDeviceProperties2(device, &deviceProperties2);
		m_RayTracingPipelineProperties = rayTracingPipelineProperties;

		if (!vulkanPhysicalDevice12Features.descriptorIndexing)
		{
			TYR_ASSERT(false);
			TYR_LOG_FATAL("Descriptor indexing is not supported by the GPU.");
		}

		if (!vulkanPhysicalDeviceMeshShaderFeatures.meshShader || !vulkanPhysicalDeviceMeshShaderFeatures.taskShader)
		{
			TYR_ASSERT(false);
			TYR_LOG_FATAL("Mesh shaders are not supported by the GPU.");
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

		// Vertex/ShaderVertex store their UV as a half2 in a storage buffer (StructuredBuffer),
		// which needs this to be readable at all.
		if (!vulkanPhysicalDevice11Features.storageBuffer16BitAccess)
		{
			TYR_ASSERT(false);
			TYR_LOG_FATAL("16-bit storage buffer access is not supported by the GPU.");
		}

		if (!vulkanPhysicalDeviceUnifiedImageLayoutsFeatures.unifiedImageLayouts)
		{
			TYR_ASSERT(false);
			TYR_LOG_FATAL("Unified image layouts is not supported by the GPU.");
		}

		if (!vulkanPhysicalDeviceAccelerationStructureFeatures.accelerationStructure)
		{
			TYR_ASSERT(false);
			TYR_LOG_FATAL("Acceleration structures are not supported by the GPU.");
		}

		if (!vulkanPhysicalDeviceRayTracingPipelineFeatures.rayTracingPipeline)
		{
			TYR_ASSERT(false);
			TYR_LOG_FATAL("The ray tracing pipeline is not supported by the GPU.");
		}

		if (!vulkanPhysicalDeviceRayQueryFeatures.rayQuery)
		{
			TYR_ASSERT(false);
			TYR_LOG_FATAL("Ray query (inline ray tracing) is not supported by the GPU.");
		}

		if (!vulkanPhysicalDevice12Features.bufferDeviceAddress)
		{
			TYR_ASSERT(false);
			TYR_LOG_FATAL("Buffer device address is not supported by the GPU.");
		}

		// GPU-driven instance culling issues one indirect mesh task draw per visible instance,
		// with the actual draw count produced by a compute pass and read from a GPU buffer
		// rather than known on the CPU.
		if (!vulkanPhysicalDevice12Features.drawIndirectCount)
		{
			TYR_ASSERT(false);
			TYR_LOG_FATAL("Indirect draw count is not supported by the GPU.");
		}

		// A mesh shader looking up its own instance by draw slot needs the shader-visible draw
		// index this feature exposes.
		if (!vulkanPhysicalDevice11Features.shaderDrawParameters)
		{
			TYR_ASSERT(false);
			TYR_LOG_FATAL("Shader draw parameters is not supported by the GPU.");
		}

		// Only the specific Vulkan 1.1 features actually used are enabled here - passing the
		// query result straight through would turn on every 1.1 feature the GPU supports, some
		// of which have their own extra requirements that aren't satisfied elsewhere.
		VkPhysicalDeviceVulkan11Features enabledVulkan11Features{};
		enabledVulkan11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
		enabledVulkan11Features.pNext = &vulkanPhysicalDevice12Features;
		enabledVulkan11Features.storageBuffer16BitAccess = VK_TRUE;
		enabledVulkan11Features.shaderDrawParameters = VK_TRUE;

		// Only the specific mesh shader features actually used are enabled here - some of the
		// other flags the GPU reports supporting require further features that aren't enabled,
		// which device creation validation would otherwise reject.
		VkPhysicalDeviceMeshShaderFeaturesEXT enabledMeshShaderFeatures{};
		enabledMeshShaderFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
		enabledMeshShaderFeatures.pNext = &enabledVulkan11Features;
		enabledMeshShaderFeatures.taskShader = VK_TRUE;
		enabledMeshShaderFeatures.meshShader = VK_TRUE;

		VkPhysicalDeviceUnifiedImageLayoutsFeaturesKHR enabledUnifiedImageLayoutsFeatures{};
		enabledUnifiedImageLayoutsFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFIED_IMAGE_LAYOUTS_FEATURES_KHR;
		enabledUnifiedImageLayoutsFeatures.pNext = &enabledMeshShaderFeatures;
		enabledUnifiedImageLayoutsFeatures.unifiedImageLayouts = VK_TRUE;

		VkPhysicalDeviceAccelerationStructureFeaturesKHR enabledAccelerationStructureFeatures{};
		enabledAccelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
		enabledAccelerationStructureFeatures.pNext = &enabledUnifiedImageLayoutsFeatures;
		enabledAccelerationStructureFeatures.accelerationStructure = VK_TRUE;

		VkPhysicalDeviceRayTracingPipelineFeaturesKHR enabledRayTracingPipelineFeatures{};
		enabledRayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
		enabledRayTracingPipelineFeatures.pNext = &enabledAccelerationStructureFeatures;
		enabledRayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;

		VkPhysicalDeviceRayQueryFeaturesKHR enabledRayQueryFeatures{};
		enabledRayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
		enabledRayQueryFeatures.pNext = &enabledRayTracingPipelineFeatures;
		enabledRayQueryFeatures.rayQuery = VK_TRUE;

		deviceInfo.pNext = &enabledRayQueryFeatures;

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

		// Required for any buffer created with device-address usage - without this, the allocator
		// gives their memory no device-address capability, and binding them later fails.
		allocatorCI.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

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

	SwapChain* Device::CreateSwapChain(void* windowOSHandle, const SwapChainDesc& desc)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		return new VulkanSwapChain(windowOSHandle, &device, desc);
	}

	CommandQueue* Device::CreateCommandQueue(CommandQueueType queueType, uint queueIndex, const GDebugString& debugName)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		if (queueIndex >= device.GetNumQueues(queueType))
		{
			return nullptr;
		}
		return new CommandQueueInternal(device, debugName, queueType, queueIndex);
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

	// Needs to be in this file as it requires VmaAllocation's implementation to call GetSize().
	size_t Device::GetImageAllocationSize(ImageHandle handle)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		Image& image = device.GetImage(handle);
		return static_cast<size_t>(image.allocation->GetSize());
	}
}