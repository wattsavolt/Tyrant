
#pragma once

#include "RenderResource.h"
#include "RenderAPI/GraphicsBase.h"
#include "RenderAPI/RenderAPITypes.h"
#include "RenderAPI/Buffer.h"

namespace tyr
{
	class Device;
	class CommandList;
	class TransferBuffer;
	struct BufferBarrier;

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
		String debugName;
		GpuBufferUsage usage = GpuBufferUsage::Uniform;
		size_t size = 0;
		// Stride only needed for index buffers
		uint stride = 0;
		// This can be disabled if loading direct from SSD to VRAM.
		bool useTransferBuffer = true;
	};
	
	/// Class that represents a staging and gpu buffer (upload and gpu buffer)
	class TYR_RENDERER_EXPORT GpuBuffer final : public RenderResource
	{
	public:
		GpuBuffer(Device& device, const GpuBufferDesc& desc);
		~GpuBuffer();

		// Should only be called when the buffer is no longer in use
		void Recreate(const GpuBufferDesc& desc);

		static Ref<GpuBuffer> Create(Device& device, const GpuBufferDesc& desc);

		void CreateTransferBarriers(BufferBarrier& transferBarrier, BufferBarrier& gpuBarrier, uint srcQueueFamilyIndex = QUEUE_FAMILY_IGNORED);

		void CreateTransferBarrier(BufferBarrier& gpuBarrier, uint srcQueueFamilyIndex = QUEUE_FAMILY_IGNORED);

		// Writes the data to the transfer buffer if using one
		void Write(const void* data, size_t offset, size_t size);

		// Transfers all data from the transfer buffer to the gpu buffer
		void Upload(Ref<CommandList>& commandList);

		Ref<TransferBuffer>& GetTransferBuffer() { return m_TransferBuffer; }

		ORef<Buffer>& GetBuffer() { return m_Buffer; }
		const Buffer* GetBufferRawPtr() const { return m_Buffer.Get(); }
		Buffer* GetBufferRawPtr() { return m_Buffer.Get(); }
		void* GetBufferNativePtr() const { return m_Buffer->GetNativePtr(); }

		ORef<BufferView>& GetBufferView() { return m_BufferView; }
		const BufferView* GetBufferViewRawPtr() const { return m_BufferView.Get(); }
		BufferView* GetBufferViewRawPtr() { return m_BufferView.Get(); }
		
	private:
		void CreateTransferBuffer(const GpuBufferDesc& desc);
		void CreateBufferAndView(const GpuBufferDesc& desc);

		// Buffer in cpu memory.
		Ref<TransferBuffer> m_TransferBuffer;
		// buffer in gpu memory.
		ORef<Buffer> m_Buffer;
		ORef<BufferView> m_BufferView;
		bool m_UseTransferBuffer;
	};
}
