#pragma once

#include "GraphicsBase.h"
#include "RenderAPI/RenderAPITypes.h"
#include "RenderAPI/Buffer.h"
#include "RenderAPI/Image.h"
#include "RenderAPI/AccelerationStructure.h"
#include "RenderAPI/DescriptorSetGroup.h"
#include "RenderAPI/ShaderModule.h"
#include "RenderAPI/Sync.h"
#include "RenderAPI/Pipeline.h"

namespace tyr
{
	struct CommandAllocatorDesc;
	struct CommandListDesc;
	class CommandAllocator;
	class CommandList;

	struct DeviceProperties
	{
		uint maxStorageBufferRange;
	};

	/// Class repesenting a logical device 
	class TYR_GRAPHICS_EXPORT Device
	{
	public:
		static constexpr uint16 c_MaxBuffers = 20;
		static constexpr uint16 c_MaxBufferViews = c_MaxBuffers * 3;
		static constexpr uint16 c_MaxImages = 3000;
		static constexpr uint16 c_MaxImageViews = c_MaxImages * 3;
		static constexpr uint16 c_MaxSamplers = 20;
		static constexpr uint16 c_MaxRenderPasses = 1;
		static constexpr uint16 c_MaxGraphicsPipelines = 10;
		static constexpr uint16 c_MaxComputePipelines = 10;
		static constexpr uint16 c_MaxRayTracingPipelines = 10;
		static constexpr uint16 c_MaxAccelerationStructures = 20;
		static constexpr uint16 c_MaxDescriptorPools = 5;
		static constexpr uint16 c_MaxDescriptorSetLayouts = 20;
		static constexpr uint16 c_MaxDescriptorSetGroups = 20;
		static constexpr uint16 c_MaxFences = 20;
		static constexpr uint16 c_MaxSemaphores = 20;
		static constexpr uint16 c_MaxEvents = 20;
		static constexpr uint16 c_MaxShaderModules = 50;

		Device(uint index);
		virtual ~Device();
		
		// No virtuals as implementations determined at compile time for faster performance
		CommandAllocator* CreateCommandAllocator(const CommandAllocatorDesc& desc);
		CommandList* CreateCommandList(const CommandListDesc& desc);
		BufferHandle CreateBuffer(const BufferDesc& desc);
		void DeleteBuffer(BufferHandle handle);
		BufferViewHandle CreateBufferView(const BufferViewDesc& desc);
		void DeleteBufferView(BufferViewHandle handle);
		ImageHandle CreateImage(const ImageDesc& desc);
		void DeleteImage(ImageHandle handle);
		ImageViewHandle CreateImageView(const ImageViewDesc& desc);
		void DeleteImageView(ImageViewHandle handle);
		SamplerHandle CreateSampler(const SamplerDesc& desc);
		void DeleteSampler(SamplerHandle handle);
		RenderPassHandle CreateRenderPass(const RenderPassDesc& desc);
		void DeleteRenderPass(RenderPassHandle handle);
		GraphicsPipelineHandle CreateGraphicsPipeline(const GraphicsPipelineDesc& desc);
		void DeleteGraphicsPipeline(GraphicsPipelineHandle handle);
		ComputePipelineHandle CreateComputePipeline(const ComputePipelineDesc& desc);
		void DeleteComputePipeline(ComputePipelineHandle handle);
		RayTracingPipelineHandle CreateRayTracingPipeline(const RayTracingPipelineDesc& desc);
		void DeleteRayTracingPipeline(RayTracingPipelineHandle handle);
		AccelerationStructureHandle CreateAccelerationStructure(const AccelerationStructureDesc& desc);	
		void DeleteAccelerationStructure(AccelerationStructureHandle handle);
		DescriptorPoolHandle CreateDescriptorPool(const DescriptorPoolDesc& desc);
		void DeleteDescriptorPool(DescriptorPoolHandle handle);
		DescriptorSetLayoutHandle CreateDescriptorSetLayout(const DescriptorSetLayoutDesc& desc);
		void DeleteDescriptorSetLayout(DescriptorSetLayoutHandle handle);
		// Potentially add CreateDescriptorSetGroups function that can batch allocate multiple group sets on vulkan
		DescriptorSetGroupHandle CreateDescriptorSetGroup(const DescriptorSetGroupDesc& desc);
		void DeleteDescriptorSetGroup(DescriptorSetGroupHandle handle);
		FenceHandle CreateFence(const FenceDesc& desc);
		void DeleteFence(FenceHandle handle);
		// "Resource" is appended to the name of the below four functions to avoid conflict with winapi functions
		SemaphoreHandle CreateSemaphoreResource(const SemaphoreDesc& desc);
		void DeleteSemaphoreResource(SemaphoreHandle handle);
		EventHandle CreateEventResource(const EventDesc& desc);
		void DeleteEventResource(EventHandle handle);
		ShaderModuleHandle CreateShaderModule(const ShaderModuleDesc& desc);
		void DeleteShaderModule(ShaderModuleHandle handle);

		// Gets the size of the buffer in bytes
		size_t GetBufferSize(BufferHandle handle) const;
		// Gets the size of the allocation made for the buffer (can be bigger than the size)
		size_t GetBufferAllocationSize(BufferHandle handle) const;
		uint8* MapBuffer(BufferHandle handle);
		void UnmapBuffer(BufferHandle handle);
		void WriteBuffer(BufferHandle handle, const void* data, size_t offset, size_t size);
		void ReadBuffer(BufferHandle handle, void* data, size_t offset, size_t size);

		size_t GetImageAllocationSize(ImageHandle handle);

		void UpdateDescriptorSetGroup(DescriptorSetGroupHandle handle, const BufferBindingUpdate* bufferUpdates, uint bufferUpdateCount, const ImageBindingUpdate* imageUpdates = nullptr,
			uint imageUpdateCount = 0, const AccelerationStructureBindingUpdate* accelerationStructureUpdates = nullptr, uint accelerationStructureUpdateCount = 0);

		void ResetFence(FenceHandle handle);
		bool GetFenceStatus(FenceHandle handle) const;
		bool WaitForFence(FenceHandle handle, uint64 timeout) const;

		void SignalSemaphore(SemaphoreHandle handle, uint64 value);
		uint64 GetSemaphoreValue(SemaphoreHandle handle) const;
		bool WaitForSemaphore(SemaphoreHandle handle, uint64 value, uint64 timeout) const;

		bool SetEvent(EventHandle handle);
		void ResetEvent(EventHandle handle);
		bool IsEventSet(EventHandle handle) const;

		ShaderBinaryLanguage GetShaderBinaryLanguage() const;

		/// Blocks the calling thread until all operations on the device are complete. 
		void WaitIdle() const;
		bool IsDiscreteGPU() const;

		uint GetIndex() const { return m_Index; }

		const DeviceProperties& GetProperties() const { return m_DeviceProperties; }

		// Finds what requested memory property flags the devuce has
		uint FindMemoryType(uint requirementBits, MemoryProperty requestedFlags) const;

		// Checks if the device has a specific memory property
		bool HasMemoryType(MemoryProperty requestedFlags) const;

	protected:
		// Physical device properties
		DeviceProperties m_DeviceProperties;
		// Index of the physical device
		uint m_Index;
	};
}
