#pragma once

#include "GraphicsBase.h"
#include "RenderAPI/RenderAPITypes.h"
#include "RenderAPI/Buffer.h"
#include "RenderAPI/Image.h"
#include "RenderAPI/AccelerationStructure.h"
#include "RenderAPI/DescriptorSet.h"
#include "RenderAPI/ShaderModule.h"
#include "RenderAPI/Sync.h"
#include "RenderAPI/Pipeline.h"

namespace tyr
{
	struct SwapChainDesc;
	struct CommandAllocatorDesc;
	struct CommandListDesc;
	class SwapChain;
	class CommandAllocator;
	class CommandList;
	class CommandQueue;

	struct DeviceProperties
	{
		uint maxStorageBufferRange;
	};

	/// Class repesenting a logical device 
	class TYR_GRAPHICS_API Device
	{
	public:
		// Every raw Vulkan buffer the whole engine ever creates shares this one pool, including
		// one acceleration-structure backing buffer per mesh - sized well above the handful of
		// long-lived buffers a renderer typically needs on its own.
		static constexpr uint16 c_MaxBuffers = 2048;
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
		static constexpr uint16 c_MaxDescriptorSets = 20;
		static constexpr uint16 c_MaxFences = 20;
		static constexpr uint16 c_MaxSemaphores = 20;
		static constexpr uint16 c_MaxEvents = 20;
		static constexpr uint16 c_MaxShaderModules = 50;

		Device(uint index);
		virtual ~Device();
		
		// No virtuals as implementations determined at compile time for faster performance
		SwapChain* CreateSwapChain(void* windowOSHandle, const SwapChainDesc& desc);
		// Returns nullptr if this device has no queue of the requested type (a dedicated
		// compute/transfer queue isn't guaranteed to exist).
		CommandQueue* CreateCommandQueue(CommandQueueType queueType, uint queueIndex, const GDebugString& debugName);
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
		// Creates its own dedicated backing buffer, sized exactly to this structure - fine for a
		// handful of long-lived structures, but wasteful for thousands of small ones, since each
		// is a separate allocation and most Vulkan implementations cap the total allocation count.
		AccelerationStructureHandle CreateAccelerationStructure(const AccelerationStructureDesc& desc);
		// Creates into caller-owned storage instead of a dedicated buffer - backingBuffer must
		// stay alive and that byte range must not be reused for as long as the returned handle
		// lives; deleting the structure won't free backingBuffer itself.
		AccelerationStructureHandle CreateAccelerationStructureAt(const AccelerationStructureDesc& desc, BufferHandle backingBuffer, size_t backingOffset);
		// Sizes this structure would need without creating anything - outASSize already includes
		// required build-size alignment, though a caller-chosen backing offset still needs its
		// own acceleration-structure alignment on top.
		void GetAccelerationStructureSize(const AccelerationStructureDesc& desc, size_t& outASSize, size_t& outBuildScratchSize) const;
		void DeleteAccelerationStructure(AccelerationStructureHandle handle);
		uint64 GetAccelerationStructureDeviceAddress(AccelerationStructureHandle handle) const;
		// How large a scratch buffer BuildAccelerationStructures needs for this acceleration
		// structure - build (initial construction) is always usable, update (fast refit) only if
		// the structure was created with ACCELERATION_STRUCTURE_BUILD_ALLOW_UPDATE_BIT.
		size_t GetAccelerationStructureBuildScratchSize(AccelerationStructureHandle handle) const;
		size_t GetAccelerationStructureUpdateScratchSize(AccelerationStructureHandle handle) const;
		// Every scratch buffer offset passed to BuildAccelerationStructures must be a multiple of
		// this - implementation-defined, not a fixed spec constant like acceleration structure
		// storage's own 256-byte offset alignment.
		size_t GetAccelerationStructureScratchOffsetAlignment() const;
		DescriptorPoolHandle CreateDescriptorPool(const DescriptorPoolDesc& desc);
		void DeleteDescriptorPool(DescriptorPoolHandle handle);
		DescriptorSetLayoutHandle CreateDescriptorSetLayout(const DescriptorSetLayoutDesc& desc);
		void DeleteDescriptorSetLayout(DescriptorSetLayoutHandle handle);
		DescriptorSetHandle CreateDescriptorSet(const DescriptorSetDesc& desc);
		void DeleteDescriptorSet(DescriptorSetHandle handle);
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
		void* MapBuffer(BufferHandle handle);
		void UnmapBuffer(BufferHandle handle);
		void WriteBuffer(BufferHandle handle, const void* data, size_t offset, size_t size);
		void FlushBufferAllocation(BufferHandle handle, size_t offset, size_t size);
		void ReadBuffer(BufferHandle handle, void* data, size_t offset, size_t size);
		// Buffer must have been created with BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT set - needed
		// for acceleration structure geometry/instance input and shader binding table buffers.
		uint64 GetBufferDeviceAddress(BufferHandle handle) const;

		size_t GetImageAllocationSize(ImageHandle handle);

		// How much GPU-local memory the app uses and may use, as the OS reports it when it can.
		// Changes over time, since other apps share the GPU.
		void GetDeviceMemoryBudget(size_t& outUsage, size_t& outBudget);

		// Once a frame, so the memory budget stays up to date.
		void SetCurrentFrameIndex(uint frameIndex);

		void UpdateDescriptorSet(DescriptorSetHandle handle, const BufferBindingUpdate* bufferUpdates, uint bufferUpdateCount, const ImageBindingUpdate* imageUpdates = nullptr,
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

	private:
		// Shared tail end of CreateAccelerationStructure/CreateAccelerationStructureAt, once each
		// has settled on a backing buffer/offset - implemented in the active graphics backend,
		// same as every other method here.
		AccelerationStructureHandle CreateAccelerationStructureIntoBuffer(const AccelerationStructureDesc& desc, BufferHandle backingBuffer, size_t backingOffset, bool externalBackingBuffer);

	protected:
		// Physical device properties
		DeviceProperties m_DeviceProperties;
		// Index of the physical device
		uint m_Index;
	};
}
