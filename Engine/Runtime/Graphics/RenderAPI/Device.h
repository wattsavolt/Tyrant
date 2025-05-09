
#pragma once

#include "GraphicsBase.h"
#include "RenderAPI/RenderAPITypes.h"

namespace tyr
{
	struct RenderPassDesc;
	struct GraphicsPipelineDesc;
	struct ComputePipelineDesc;
	struct RayTracingPipelineDesc;
	struct CommandAllocatorDesc;
	struct CommandListDesc;
	struct AccelerationStructureDesc;
	struct BufferDesc;
	struct BufferViewDesc;
	struct ImageDesc;
	struct ImageViewDesc;
	struct SamplerDesc;
	struct ShaderDesc;
	struct DescriptorPoolDesc;
	struct DescriptorSetLayoutDesc;
	struct DescriptorSetGroupDesc;
	struct FenceDesc;
	struct SemaphoreDesc;
	struct EventDesc;
	class RenderPass;
	class GraphicsPipeline;
	class ComputePipeline;
	class RayTracingPipeline;
	class CommandAllocator;
	class CommandList;
	class AccelerationStructure;
	class Buffer;
	class BufferView;
	class Image;
	class ImageView;
	class Sampler;
	class Shader;
	class DescriptorPool;
	class DescriptorSetLayout;
	class DescriptorSetGroup;
	class Fence;
	class Semaphore;
	class Event;

	struct DeviceProperties
	{
		uint maxStorageBufferRange;
	};

	/// Class repesenting a logical device 
	class TYR_GRAPHICS_EXPORT Device
	{
	public:
		Device(uint index);
		virtual ~Device();
		
		virtual Ref<RenderPass> CreateRenderPass(const RenderPassDesc& desc) = 0;
		virtual Ref<GraphicsPipeline> CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) = 0;
		virtual Ref<ComputePipeline> CreateComputePipeline(const ComputePipelineDesc& desc) = 0;
		virtual Ref<RayTracingPipeline> CreateRayTracingPipeline(const RayTracingPipelineDesc& desc) = 0;
		virtual Ref<CommandAllocator> CreateCommandAllocator(const CommandAllocatorDesc& desc) = 0;
		virtual Ref<CommandList> CreateCommandList(const CommandListDesc& desc) = 0;
		virtual Ref<AccelerationStructure> CreateAccelerationStructure(const AccelerationStructureDesc& desc) = 0;
		virtual Ref<Buffer> CreateBuffer(const BufferDesc& desc) = 0;
		virtual Ref<BufferView> CreateBufferView(const BufferViewDesc& desc) = 0;
		virtual Ref<Image> CreateImage(const ImageDesc& desc) = 0;
		virtual Ref<ImageView> CreateImageView(const ImageViewDesc& desc) = 0;
		virtual Ref<Sampler> CreateSampler(const SamplerDesc& desc) = 0;
		virtual Ref<Shader> CreateShader(const ShaderDesc& desc) = 0;
		virtual Ref<DescriptorPool> CreateDescriptorPool(const DescriptorPoolDesc& desc, const char* debugName = "") = 0;
		virtual Ref<DescriptorSetLayout> CreateDescriptorSetLayout(const DescriptorSetLayoutDesc& desc, const char* debugName = "") = 0;
		virtual Ref<DescriptorSetGroup> CreateDescriptorSetGroup(const DescriptorSetGroupDesc& desc, const char* debugName = "") = 0;
		virtual Ref<Fence> CreateFence(const FenceDesc& desc) = 0;
		virtual Ref<Semaphore> CreateSemaphoreResource(const SemaphoreDesc& desc) = 0;
		virtual Ref<Event> CreateEventResource(const EventDesc& desc) = 0;

		/// Blocks the calling thread until all operations on the device are complete. 
		virtual void WaitIdle() = 0;
		virtual bool IsDiscreteGPU() const = 0;

		virtual uint GetNumQueues(CommandQueueType type) const = 0;

		uint GetIndex() const { return m_Index; }

		const DeviceProperties& GetProperties() const { return m_Properties; }

		// Finds what requested memory property flags the devuce has
		virtual uint FindMemoryType(uint requirementBits, MemoryProperty requestedFlags) const = 0;

		// Checks if the device has a specific memory property
		virtual bool HasMemoryType(MemoryProperty requestedFlags) const = 0;

		/// Returns queue family index for a given queue type (vulkan only).
		virtual uint GetQueueFamilyIndex(CommandQueueType type) const;

	protected:
		// Physical device properties
		DeviceProperties m_Properties;
		// Index of the physical device
		uint m_Index;
	};
}
