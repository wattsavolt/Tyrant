#pragma once

#include "RenderAPI/GraphicsBase.h"
#include "RenderAPI/RenderAPITypes.h"
#include "RenderAPI/Buffer.h"
#include "RenderAPI/Sync.h"
#include "RenderResource.h"

namespace tyr
{
	class Device;
	struct BufferBarrier;

	struct TransferBufferDesc
	{
#if !TYR_FINAL 
		GDebugString debugName;
#endif
		size_t size = 0;
		// Following three members are only needed for transfer buffers used for textures (for future D3D12 compatibility)
		uint imageWidth = 0;
		uint imageHeight = 0;
		uint pixelSize = 0;
	};

	enum class GpuBufferUsage
	{
		Uknown = 0,
		UniformTexel,
		StorageTexel,
		Uniform,
		Storage,
		Index,
		Vertex,
		IndexAndVertex,
		RayTracing
	};

	struct GpuBufferDesc
	{
#if !TYR_FINAL 
		GDebugString debugName;
#endif
		GpuBufferUsage usage = GpuBufferUsage::Uniform;
		size_t size = 0;
		// Stride only needed for index buffers
		uint stride = 0;
	};
	
	struct RenderBuffer : public RenderResource
	{
		BufferHandle buffer;
		BufferViewHandle bufferView;
	};

	/// Class that represents a staging and gpu buffer (upload and gpu buffer)
	class TYR_RENDERER_EXPORT RenderBufferUtil final
	{
	public:
		static void CreateTransferBuffer(RenderBuffer& buffer, Device& device, const TransferBufferDesc& desc);

		static void CreateGpuBuffer(RenderBuffer& buffer, Device& device, const GpuBufferDesc& desc);

		static void DeleteBuffer(RenderBuffer& buffer, Device& device);

		static void CreateTransferWriteBarrier(BufferBarrier& barrier, BufferHandle buffer, size_t size = TYR_BUFFER_BARRIER_WHOLE_SIZE, uint srcQueueFamilyIndex = QUEUE_FAMILY_IGNORED);

		static void CreateTransferReadBarrier(BufferBarrier& barrier, BufferHandle buffer, size_t size = TYR_BUFFER_BARRIER_WHOLE_SIZE, uint srcQueueFamilyIndex = QUEUE_FAMILY_IGNORED);
	};
}
