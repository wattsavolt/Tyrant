
#pragma once

#include "VulkanUtility.h"
#include "RenderAPI/Device.h"

namespace tyr
{
	struct VulkanSurfaceFormat
	{
		VkFormat colorFormat;
		VkFormat depthFormat;
		VkColorSpaceKHR colorSpace;
	};

	class VulkanDevice final : public Device
	{
	public:
		VulkanDevice(VkInstance instance, VkPhysicalDevice device, uint index);
		~VulkanDevice();

		void WaitIdle() override;

		VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }

		VkDevice GetLogicalDevice() const { return m_LogicalDevice; }

		/// Returns the number of queue supported on the device, per type. 
		uint GetNumQueues(CommandQueueType type) const override { return (uint)m_QueueGroups[(int)type].queues.Size(); }

		/// Returns queue of the specified type at the specified index. Index must be in range [0, GetNumQueues()]. 
		VkQueue GetQueue(CommandQueueType type, uint index) const { return m_QueueGroups[(int)type].queues[index]; }

		uint FindMemoryType(uint requirementBits, MemoryProperty requestedFlags) const override;

		bool HasMemoryType(MemoryProperty requestedFlags) const override;

		uint GetQueueFamilyIndex(CommandQueueType type) const override { return m_QueueGroups[(int)type].familyIndex; }

		/// Returns a set of properties describing the physical device. 
		const VkPhysicalDeviceProperties& GetDeviceProperties() const { return m_DeviceProperties; }

		/// Returns a set of features that the application can use to check if a specific feature is supported. 
		const VkPhysicalDeviceFeatures& GetDeviceFeatures() const { return m_DeviceFeatures; }

		/// Returns a set of properties describing the memory of the physical device. 
		const VkPhysicalDeviceMemoryProperties& GetMemoryProperties() const { return m_MemoryProperties; }

		/// Allocates memory for the provided buffer, and binds it to the buffer. Returns null if it cannot find memory
		/// using the property flags provided.
		VmaAllocation AllocateMemory(VkBuffer buffer, VkMemoryPropertyFlags flags, size_t& size);

		/// Allocates memory for an image and binds it. Returns null if it cannot find memory
		/// with the specified flags.
		VmaAllocation AllocateMemory(VkImage image, VkMemoryPropertyFlags flags, size_t& size);

		/// Frees an allocated memory block.
		void FreeMemory(VmaAllocation allocation);

		/// Returns the device memory block and offset into the block for an allocation.
		void GetAllocationInfo(VmaAllocation allocation, VkDeviceMemory& memory, VkDeviceSize& offset);

		/////// Resource API functions ///////

		Ref<RenderPass> CreateRenderPass(const RenderPassDesc& desc) override;
		Ref<GraphicsPipeline> CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) override;
		Ref<ComputePipeline> CreateComputePipeline(const ComputePipelineDesc& desc) override;
		Ref<RayTracingPipeline> CreateRayTracingPipeline(const RayTracingPipelineDesc& desc) override;
		Ref<CommandAllocator> CreateCommandAllocator(const CommandAllocatorDesc& desc) override;
		Ref<CommandList> CreateCommandList(const CommandListDesc& desc) override;
		Ref<AccelerationStructure> CreateAccelerationStructure(const AccelerationStructureDesc& desc) override;
		Ref<Buffer> CreateBuffer(const BufferDesc& desc) override;
		Ref<BufferView> CreateBufferView(const BufferViewDesc& desc) override;
		Ref<Image> CreateImage(const ImageDesc& desc) override;
		Ref<ImageView> CreateImageView(const ImageViewDesc& desc) override;
		Ref<Sampler> CreateSampler(const SamplerDesc& desc) override;
		Ref<Shader> CreateShader(const ShaderDesc& desc) override;
		Ref<DescriptorPool> CreateDescriptorPool(const DescriptorPoolDesc& desc, const char* debugName) override;
		Ref<DescriptorSetLayout> CreateDescriptorSetLayout(const DescriptorSetLayoutDesc& desc, const char* debugName = "") override;
		Ref<DescriptorSetGroup> CreateDescriptorSetGroup(const DescriptorSetGroupDesc& desc, const char* debugName = "") override;
		Ref<Fence> CreateFence(const FenceDesc& desc) override;
		Ref<Semaphore> CreateSemaphoreResource(const SemaphoreDesc& desc) override;
		Ref<Event> CreateEventResource(const EventDesc& desc) override;

		static const Array<const char*> c_RequiredDeviceExtensions;

		VmaAllocator GetAllocator() { return m_Allocator; }

		/// Checks if the device is a discrete GPU (not an integrated GPU).
		bool IsDiscreteGPU() const override
		{ 
			return m_DeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
		}
		

	private:
		VkPhysicalDevice m_PhysicalDevice;
		VkDevice m_LogicalDevice = VK_NULL_HANDLE;

		VmaAllocator m_Allocator;

		VkPhysicalDeviceProperties m_DeviceProperties;
		VkPhysicalDeviceFeatures m_DeviceFeatures;
		VkPhysicalDeviceMemoryProperties m_MemoryProperties;

		/// Contains data about a set of queues of a specific type. 
		struct VulkanQueueGroup
		{
			uint familyIndex;
			Array<VkQueue> queues;
		};

		static constexpr uint c_QueueGroupCount = (uint)CQ_COUNT;
		VulkanQueueGroup m_QueueGroups[c_QueueGroupCount];

		static constexpr uint c_MaxQueuesPerType = 8;
	};
}
