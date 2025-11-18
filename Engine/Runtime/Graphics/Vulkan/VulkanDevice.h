#pragma once

#include "RenderAPI/Device.h"
#include "VulkanUtility.h"
#include "VulkanBuffer.h"
#include "VulkanImage.h"
#include "VulkanPipeline.h"
#include "VulkanAccelerationStructure.h"
#include "VulkanShaderModule.h"
#include "VulkanDescriptorSetGroup.h"
#include "VulkanSync.h"
#include "Memory/LocalObjectPool.h"

namespace tyr
{
	struct VulkanSurfaceFormat
	{
		VkFormat colorFormat;
		VkFormat depthFormat;
		VkColorSpaceKHR colorSpace;
	};

	class DeviceInternal final : public Device
	{
	public:
		DeviceInternal(VkInstance instance, VkPhysicalDevice device, uint index);
		~DeviceInternal();

		VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }

		VkDevice GetLogicalDevice() const { return m_LogicalDevice; }

		/// Returns the number of queue supported on the device, per type. 
		uint GetNumQueues(CommandQueueType type) const { return (uint)m_QueueGroups[(int)type].queues.Size(); }

		/// Returns queue of the specified type at the specified index. Index must be in range [0, GetNumQueues()]. 
		VkQueue GetQueue(CommandQueueType type, uint index) const { return m_QueueGroups[(int)type].queues[index]; }

		uint GetQueueFamilyIndex(CommandQueueType type) const { return m_QueueGroups[(int)type].familyIndex; }

		/// Returns a set of properties describing the physical device. 
		const VkPhysicalDeviceProperties& GetDeviceProperties() const { return m_VulkanDeviceProperties; }

		/// Returns a set of features that the application can use to check if a specific feature is supported. 
		const VkPhysicalDeviceFeatures& GetDeviceFeatures() const { return m_VulkanDeviceFeatures; }

		/// Returns a set of properties describing the memory of the physical device. 
		const VkPhysicalDeviceMemoryProperties& GetMemoryProperties() const { return m_VulkanMemoryProperties; }

		/// Allocates memory for the provided buffer, and binds it to the buffer. Returns null if it cannot find memory
		/// using the property flags provided.
		VmaAllocation AllocateMemory(VkBuffer buffer, VkMemoryPropertyFlags flags);

		/// Allocates memory for an image and binds it. Returns null if it cannot find memory
		/// with the specified flags.
		VmaAllocation AllocateMemory(VkImage image, VkMemoryPropertyFlags flags);

		/// Frees an allocated memory block.
		void FreeMemory(VmaAllocation allocation);

		/// Returns the device memory block and offset into the block for an allocation.
		void GetAllocationInfo(VmaAllocation allocation, VkDeviceMemory& memory, VkDeviceSize& offset);

		VmaAllocator GetAllocator() { return m_Allocator; }
		
		static const Array<const char*> c_RequiredDeviceExtensions;

	private:
		friend class Device;

		static constexpr uint c_QueueGroupCount = (uint)CQ_COUNT;
		static constexpr uint c_MaxQueuesPerType = 8;

		/// Contains data about a set of queues of a specific type. 
		struct VulkanQueueGroup
		{
			uint familyIndex;
			LocalArray<VkQueue, c_MaxQueuesPerType> queues;
		};

		VkPhysicalDevice m_PhysicalDevice;
		VkDevice m_LogicalDevice = VK_NULL_HANDLE;

		VmaAllocator m_Allocator;

		VkPhysicalDeviceProperties m_VulkanDeviceProperties;
		VkPhysicalDeviceFeatures m_VulkanDeviceFeatures;
		VkPhysicalDeviceMemoryProperties m_VulkanMemoryProperties;	

		VulkanQueueGroup m_QueueGroups[c_QueueGroupCount];

		LocalObjectPool<Buffer, c_MaxBuffers> m_BufferPool;
		LocalObjectPool<BufferView, c_MaxBufferViews> m_BufferViewPool;
		LocalObjectPool<Image, c_MaxImages> m_ImagePool;
		LocalObjectPool<ImageView, c_MaxImageViews> m_ImageViewPool;
		LocalObjectPool<Sampler, c_MaxSamplers> m_SamplerPool;
		LocalObjectPool<RenderPass, c_MaxRenderPasses> m_RenderPassPool;
		LocalObjectPool<GraphicsPipeline, c_MaxGraphicsPipelines> m_GraphicsPipelinePool;
		LocalObjectPool<ComputePipeline, c_MaxComputePipelines> m_ComputePipelinePool;
		LocalObjectPool<RayTracingPipeline, c_MaxRayTracingPipelines> m_RayTracingPipelinePool;
		LocalObjectPool<AccelerationStructure, c_MaxAccelerationStructures> m_AccelerationStructurePool;
		LocalObjectPool<DescriptorPool, c_MaxDescriptorPools> m_DescriptorPoolPool;
		LocalObjectPool<DescriptorSetLayout, c_MaxDescriptorSetLayouts> m_DescriptorSetLayoutPool;
		LocalObjectPool<DescriptorSetGroup, c_MaxDescriptorSetGroups> m_DescriptorSetGroupPool;
		LocalObjectPool<Fence, c_MaxFences> m_FencePool;
		LocalObjectPool<Semaphore, c_MaxSemaphores> m_SemaphorePool;
		LocalObjectPool<Event, c_MaxEvents> m_EventPool;
		LocalObjectPool<ShaderModule, c_MaxShaderModules> m_ShaderModulePool;

	public:
		TYR_FORCEINLINE	const Buffer& GetBuffer(BufferHandle handle) const
		{
			return m_BufferPool.GetObjectRef(handle.id);
		}

		TYR_FORCEINLINE	Buffer& GetBuffer(BufferHandle handle)
		{
			return m_BufferPool.GetObjectRef(handle.id);
		}

		TYR_FORCEINLINE	const BufferView& GetBufferView(BufferViewHandle handle) const
		{
			return m_BufferViewPool.GetObjectRef(handle.id);
		}

		TYR_FORCEINLINE	BufferView& GetBufferView(BufferViewHandle handle)
		{
			return m_BufferViewPool.GetObjectRef(handle.id);
		}

		TYR_FORCEINLINE	const Image& GetImage(ImageHandle handle) const
		{
			return m_ImagePool.GetObjectRef(handle.id);
		}

		TYR_FORCEINLINE	Image& GetImage(ImageHandle handle)
		{
			return m_ImagePool.GetObjectRef(handle.id);
		}

		TYR_FORCEINLINE	const ImageView& GetImageView(ImageViewHandle handle) const
		{
			return m_ImageViewPool.GetObjectRef(handle.id);
		}

		TYR_FORCEINLINE	ImageView& GetImageView(ImageViewHandle handle)
		{
			return m_ImageViewPool.GetObjectRef(handle.id);
		}

		TYR_FORCEINLINE	const Sampler& GetSampler(SamplerHandle handle) const
		{
			return m_SamplerPool.GetObjectRef(handle.id);
		}

		TYR_FORCEINLINE	Sampler& GetSampler(SamplerHandle handle)
		{
			return m_SamplerPool.GetObjectRef(handle.id);
		}

		TYR_FORCEINLINE	const RenderPass& GetRenderPass(RenderPassHandle handle) const
		{
			return m_RenderPassPool.GetObjectRef(handle.id);
		}

		TYR_FORCEINLINE	RenderPass& GetRenderPass(RenderPassHandle handle)
		{
			return m_RenderPassPool.GetObjectRef(handle.id);
		}

		TYR_FORCEINLINE	const GraphicsPipeline& GetGraphicsPipeline(GraphicsPipelineHandle handle) const
		{
			return m_GraphicsPipelinePool.GetObjectRef(handle.id);
		}

		TYR_FORCEINLINE	GraphicsPipeline& GetGraphicsPipeline(GraphicsPipelineHandle handle)
		{
			return m_GraphicsPipelinePool.GetObjectRef(handle.id);
		}

		TYR_FORCEINLINE	const ComputePipeline& GetComputePipeline(ComputePipelineHandle handle) const
		{
			return m_ComputePipelinePool.GetObjectRef(handle.id);
		}

		TYR_FORCEINLINE	ComputePipeline& GetComputePipeline(ComputePipelineHandle handle)
		{
			return m_ComputePipelinePool.GetObjectRef(handle.id);
		}

		TYR_FORCEINLINE	const RayTracingPipeline& GetRayTracingPipeline(RayTracingPipelineHandle handle) const
		{
			return m_RayTracingPipelinePool.GetObjectRef(handle.id);
		}

		TYR_FORCEINLINE	RayTracingPipeline& GetRayTracingPipeline(RayTracingPipelineHandle handle)
		{
			return m_RayTracingPipelinePool.GetObjectRef(handle.id);
		}

		TYR_FORCEINLINE	const AccelerationStructure& GetAccelerationStructure(AccelerationStructureHandle handle) const
		{
			return m_AccelerationStructurePool.GetObjectRef(handle.id);
		}

		TYR_FORCEINLINE	AccelerationStructure& GetAccelerationStructure(AccelerationStructureHandle handle)
		{
			return m_AccelerationStructurePool.GetObjectRef(handle.id);
		}

		TYR_FORCEINLINE	const DescriptorPool& GetDescriptorPool(DescriptorPoolHandle handle) const
		{
			return m_DescriptorPoolPool.GetObjectRef(handle.id);
		}

		TYR_FORCEINLINE	DescriptorPool& GetDescriptorPool(DescriptorPoolHandle handle)
		{
			return m_DescriptorPoolPool.GetObjectRef(handle.id);
		}

		TYR_FORCEINLINE	const DescriptorSetLayout& GetDescriptorSetLayout(DescriptorSetLayoutHandle handle) const
		{
			return m_DescriptorSetLayoutPool.GetObjectRef(handle.id);
		}

		TYR_FORCEINLINE	DescriptorSetLayout& GetDescriptorSetLayout(DescriptorSetLayoutHandle handle)
		{
			return m_DescriptorSetLayoutPool.GetObjectRef(handle.id);
		}

		TYR_FORCEINLINE	const DescriptorSetGroup& GetDescriptorSetGroup(DescriptorSetGroupHandle handle) const
		{
			return m_DescriptorSetGroupPool.GetObjectRef(handle.id);
		}

		TYR_FORCEINLINE	DescriptorSetGroup& GetDescriptorSetGroup(DescriptorSetGroupHandle handle)
		{
			return m_DescriptorSetGroupPool.GetObjectRef(handle.id);
		}

		TYR_FORCEINLINE	const Fence& GetFence(FenceHandle handle) const
		{
			return m_FencePool.GetObjectRef(handle.id);
		}

		TYR_FORCEINLINE	Fence& GetFence(FenceHandle handle)
		{
			return m_FencePool.GetObjectRef(handle.id);
		}

		TYR_FORCEINLINE	const Semaphore& GetSemaphore(SemaphoreHandle handle) const
		{
			return m_SemaphorePool.GetObjectRef(handle.id);
		}

		TYR_FORCEINLINE	Semaphore& GetSemaphore(SemaphoreHandle handle)
		{
			return m_SemaphorePool.GetObjectRef(handle.id);
		}

		TYR_FORCEINLINE	const Event& GetEvent(EventHandle handle) const
		{
			return m_EventPool.GetObjectRef(handle.id);
		}

		TYR_FORCEINLINE	Event& GetEvent(EventHandle handle)
		{
			return m_EventPool.GetObjectRef(handle.id);
		}

		TYR_FORCEINLINE	const ShaderModule& GetShaderModule(ShaderModuleHandle handle) const
		{
			return m_ShaderModulePool.GetObjectRef(handle.id);
		}

		TYR_FORCEINLINE	ShaderModule& GetShaderModule(ShaderModuleHandle handle)
		{
			return m_ShaderModulePool.GetObjectRef(handle.id);
		}
	};
}
