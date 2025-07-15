#include "VulkanDevice.h"
#include "VulkanHelper.h"
#include "VulkanPipeline.h"
#include "VulkanCommandAllocator.h"
#include "VulkanCommandList.h"
#include "VulkanAccelerationStructure.h"
#include "VulkanBuffer.h"
#include "VulkanImage.h"
#include "VulkanShader.h"
#include "VulkanDescriptorSet.h"
#include "VulkanSync.h"
#include "Memory/ObjectPool.h"

#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

namespace tyr
{
	const Array<const char*> VulkanDevice::c_RequiredDeviceExtensions =
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

	class ThreadResourcePools
	{
	public:
		ThreadResourcePools(const ThreadResourcePoolsDesc& desc)
			: m_BufferPool(desc.bufferCount)
			, m_BufferViewPool(desc.bufferViewCount)
			, m_ImagePool(desc.imageCount)
			, m_ImageViewPool(desc.imageViewCount)
		{
		}

		void DeleteBuffer(RefCountedObject* ptr)
		{
			VulkanBuffer* buffer = static_cast<VulkanBuffer*>(ptr);
			m_BufferPool.Delete(buffer);
		}

		void DeleteBufferView(RefCountedObject* ptr)
		{
			VulkanBufferView* bufferView = static_cast<VulkanBufferView*>(ptr);
			m_BufferViewPool.Delete(bufferView);
		}

		void DeleteImage(RefCountedObject* ptr)
		{
			VulkanImage* image = static_cast<VulkanImage*>(ptr);
			m_ImagePool.Delete(image);
		}

		void DeleteImageView(RefCountedObject* ptr)
		{
			VulkanImageView* imageView = static_cast<VulkanImageView*>(ptr);
			m_ImageViewPool.Delete(imageView);
		}

		ObjectPool<VulkanBuffer>& GetBufferPool() { return m_BufferPool; }

		ObjectPool<VulkanBufferView>& GetBufferViewPool() { return m_BufferViewPool; }

		ObjectPool<VulkanImage>& GetImagePool() { return m_ImagePool; }

		ObjectPool<VulkanImageView>& GetImageViewPool() { return m_ImageViewPool; }

		static URef<ThreadResourcePools>& Instance()
		{
			TYR_THREADLOCAL static URef<ThreadResourcePools> threadResourcePools;
			return threadResourcePools;
		}

	private:
		ObjectPool<VulkanBuffer> m_BufferPool;
		ObjectPool<VulkanBufferView> m_BufferViewPool;
		ObjectPool<VulkanImage> m_ImagePool;
		ObjectPool<VulkanImageView> m_ImageViewPool;
	};

	VulkanDevice::VulkanDevice(VkInstance instance, VkPhysicalDevice device, uint index)
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
		const float defaultQueuePriorities[c_MaxQueuesPerType] = { 0.0f };
		Array<VkDeviceQueueCreateInfo> queueCreateInfos;

		auto PopulateQueueInfo = [&](CommandQueueType type, uint32_t familyIndex)
		{
			queueCreateInfos.Add(VkDeviceQueueCreateInfo());

			VkDeviceQueueCreateInfo& createInfo = queueCreateInfos.Back();
			createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			createInfo.pNext = nullptr;
			createInfo.flags = 0;
			createInfo.queueFamilyIndex = familyIndex;
			createInfo.queueCount = std::min(queueFamilyProperties[familyIndex].queueCount, (uint)c_MaxQueuesPerType);
			createInfo.pQueuePriorities = defaultQueuePriorities;
			
			m_QueueGroups[type].familyIndex = familyIndex;
			m_QueueGroups[type].queues.Resize(createInfo.queueCount, nullptr);
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
		deviceInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.Size();
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

	VulkanDevice::~VulkanDevice()
	{
		TYR_GASSERT(vkDeviceWaitIdle(m_LogicalDevice));

		vmaDestroyAllocator(m_Allocator);
		vkDestroyDevice(m_LogicalDevice, g_VulkanAllocationCallbacks);
	}

	void VulkanDevice::WaitIdle()
	{
		VkResult result = vkDeviceWaitIdle(m_LogicalDevice);
		TYR_GASSERT(result != VK_SUCCESS);
	}

	VmaAllocation VulkanDevice::AllocateMemory(VkBuffer buffer, VkMemoryPropertyFlags flags, size_t& size)
	{
		VmaAllocationCreateInfo allocCI = {};
		allocCI.requiredFlags = flags;

		VmaAllocationInfo allocInfo;
		VmaAllocation allocation;
		TYR_GASSERT(vmaAllocateMemoryForBuffer(m_Allocator, buffer, &allocCI, &allocation, &allocInfo));

		TYR_GASSERT(vkBindBufferMemory(m_LogicalDevice, buffer, allocInfo.deviceMemory, allocInfo.offset));

		if (allocation)
		{
			size = allocation->GetSize();
		}
		else
		{
			size = 0;
		}

		return allocation;
	}

	VmaAllocation VulkanDevice::AllocateMemory(VkImage image, VkMemoryPropertyFlags flags, size_t& size)
	{
		VmaAllocationCreateInfo allocCI = {};
		allocCI.requiredFlags = flags;

		VmaAllocationInfo allocInfo;
		VmaAllocation allocation;
		TYR_GASSERT(vmaAllocateMemoryForImage(m_Allocator, image, &allocCI, &allocation, &allocInfo));

		TYR_GASSERT(vkBindImageMemory(m_LogicalDevice, image, allocInfo.deviceMemory, allocInfo.offset));

		if (allocation)
		{
			size = allocation->GetSize();
		}
		else
		{
			size = 0;
		}

		return allocation;
	}

	void VulkanDevice::FreeMemory(VmaAllocation allocation)
	{
		vmaFreeMemory(m_Allocator, allocation);
	}

	void VulkanDevice::GetAllocationInfo(VmaAllocation allocation, VkDeviceMemory& memory, VkDeviceSize& offset)
	{
		VmaAllocationInfo allocInfo;
		vmaGetAllocationInfo(m_Allocator, allocation, &allocInfo);

		memory = allocInfo.deviceMemory;
		offset = allocInfo.offset;
	}

	uint VulkanDevice::FindMemoryType(uint requirementBits, MemoryProperty requestedFlags) const
	{
		const VkMemoryPropertyFlags vkRequestedFlags = static_cast<VkMemoryPropertyFlags>(requestedFlags);
		for (uint i = 0; i < m_VulkanMemoryProperties.memoryTypeCount; i++)
		{
			if (requirementBits & (1 << i))
			{
				if (Utility::HasFlag(m_VulkanMemoryProperties.memoryTypes[i].propertyFlags, vkRequestedFlags))
				{
					return i;
				}
			}
		}
		return -1;
	}

	bool VulkanDevice::HasMemoryType(MemoryProperty requestedFlags) const
	{
		const VkMemoryPropertyFlags vkRequestedFlags = static_cast<VkMemoryPropertyFlags>(requestedFlags);
		for (uint i = 0; i < m_VulkanMemoryProperties.memoryTypeCount; i++)
		{	
			if (Utility::HasFlag(m_VulkanMemoryProperties.memoryTypes[i].propertyFlags, vkRequestedFlags))
			{
				return i;
			}
		}
	}


	/////// Resource API functions ///////

	Ref<RenderPass> VulkanDevice::CreateRenderPass(const RenderPassDesc& desc)
	{
		return MakeRef<VulkanRenderPass>(this, desc);
	}

	Ref<GraphicsPipeline> VulkanDevice::CreateGraphicsPipeline(const GraphicsPipelineDesc& desc)
	{
		return MakeRef<VulkanGraphicsPipeline>(this, desc);
	}

	Ref<ComputePipeline> VulkanDevice::CreateComputePipeline(const ComputePipelineDesc& desc)
	{
		return MakeRef<VulkanComputePipeline>(this, desc);
	}

	Ref<RayTracingPipeline> VulkanDevice::CreateRayTracingPipeline(const RayTracingPipelineDesc& desc)
	{
		return MakeRef<VulkanRayTracingPipeline>(this, desc);
	}

	Ref<CommandAllocator> VulkanDevice::CreateCommandAllocator(const CommandAllocatorDesc& desc)
	{
		return MakeRef<VulkanCommandAllocator>(*this, desc);
	}

	Ref<CommandList> VulkanDevice::CreateCommandList(const CommandListDesc& desc)
	{
		return MakeRef<VulkanCommandList>(*this, desc);
	}

	Ref<AccelerationStructure> VulkanDevice::CreateAccelerationStructure(const AccelerationStructureDesc& desc)
	{
		return MakeRef<VulkanAccelerationStructure>(*this, desc);
	}

	ORef<Buffer> VulkanDevice::CreateBuffer(const BufferDesc& desc)
	{
		URef<ThreadResourcePools>& threadResourcePools = ThreadResourcePools::Instance();
		if (threadResourcePools)
		{
			static RefCountedObjectDeleter deleter = std::bind(&ThreadResourcePools::DeleteBuffer, threadResourcePools.get(), std::placeholders::_1);
			return MakeORef<VulkanBuffer>(deleter, *this, desc);
		}
		return MakeORef<VulkanBuffer>(nullptr, *this, desc);
	}

	ORef<BufferView> VulkanDevice::CreateBufferView(const BufferViewDesc& desc)
	{
		URef<ThreadResourcePools>& threadResourcePools = ThreadResourcePools::Instance();
		if (threadResourcePools)
		{
			static RefCountedObjectDeleter deleter = std::bind(&ThreadResourcePools::DeleteBufferView, threadResourcePools.get(), std::placeholders::_1);
			return MakeORef<VulkanBufferView>(deleter, *this, desc);
		}
		return MakeORef<VulkanBufferView>(nullptr, *this, desc);
	}

	ORef<Image> VulkanDevice::CreateImage(const ImageDesc& desc)
	{
		URef<ThreadResourcePools>& threadResourcePools = ThreadResourcePools::Instance();
		if (threadResourcePools)
		{
			static RefCountedObjectDeleter deleter = std::bind(&ThreadResourcePools::DeleteImage, threadResourcePools.get(), std::placeholders::_1);
			return MakeORef<VulkanImage>(deleter, *this, desc);
		}
		return MakeORef<VulkanImage>(nullptr, *this, desc);
	}

	ORef<ImageView> VulkanDevice::CreateImageView(const ImageViewDesc& desc)
	{
		URef<ThreadResourcePools>& threadResourcePools = ThreadResourcePools::Instance();
		if (threadResourcePools)
		{
			static RefCountedObjectDeleter deleter = std::bind(&ThreadResourcePools::DeleteImageView, threadResourcePools.get(), std::placeholders::_1);
			return MakeORef<VulkanImageView>(deleter, *this, desc);
		}
		return MakeORef<VulkanImageView>(nullptr, *this, desc);
	}

	Ref<Sampler> VulkanDevice::CreateSampler(const SamplerDesc& desc)
	{
		return MakeRef<VulkanSampler>(*this, desc);
	}

	Ref<Shader> VulkanDevice::CreateShader(const ShaderDesc& desc)
	{
		return MakeRef<VulkanShader>(*this, desc);
	}

	Ref<DescriptorPool> VulkanDevice::CreateDescriptorPool(const DescriptorPoolDesc& desc, const char* debugName)
	{
		return MakeRef<VulkanDescriptorPool>(*this, desc, debugName);
	}

	Ref<DescriptorSetLayout> VulkanDevice::CreateDescriptorSetLayout(const DescriptorSetLayoutDesc& desc, const char* debugName)
	{
		return MakeRef<VulkanDescriptorSetLayout>(*this, desc, debugName);
	}

	Ref<DescriptorSetGroup> VulkanDevice::CreateDescriptorSetGroup(const DescriptorSetGroupDesc& desc, const char* debugName)
	{
		return MakeRef<VulkanDescriptorSetGroup>(*this, desc, debugName);
	}

	Ref<Fence> VulkanDevice::CreateFence(const FenceDesc& desc)
	{
		return MakeRef<VulkanFence>(*this, desc);
	}

	Ref<Semaphore> VulkanDevice::CreateSemaphoreResource(const SemaphoreDesc& desc)
	{
		return MakeRef<VulkanSemaphore>(*this, desc);
	}

	Ref<Event> VulkanDevice::CreateEventResource(const EventDesc& desc)
	{
		return MakeRef<VulkanEvent>(*this, desc);
	}

	void VulkanDevice::CreateThreadResourcePools(const ThreadResourcePoolsDesc& desc)
	{
		ThreadResourcePools::Instance() = MakeURef<ThreadResourcePools>(desc);
	}
}