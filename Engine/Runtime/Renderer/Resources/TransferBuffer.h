#pragma once

#include "RenderResource.h"
#include "RenderAPI/RenderAPITypes.h"
#include "RenderAPI/Buffer.h"

namespace tyr
{
	class CommandList;
	struct BufferBarrier;

	struct TransferBufferDesc
	{
		String debugName;
		size_t size = 0;
		// Following three members are only needed for transfer buffers used for textures (for future D3D12 compatibility)
		uint imageWidth = 0;
		uint imageHeight = 0;
		uint pixelSize = 0;
	};
	
	/// Class that represents a transfer / staging buffer.
	class TYR_RENDERER_EXPORT TransferBuffer final : public RenderResource
	{
	public:
		TransferBuffer(Device& device, const TransferBufferDesc& desc);
		~TransferBuffer();

		// Should only be called when the buffer is no longer in use
		void Recreate(const TransferBufferDesc& desc);

		static Ref<TransferBuffer> Create(Device& device, const TransferBufferDesc& desc);

		void CreateTransferBarrier(BufferBarrier& barrier, uint srcQueueFamilyIndex = QUEUE_FAMILY_IGNORED);

		// Writes the data to the upload buffer if using one and gpu buffer otherwise.
		void Write(const void* data, size_t offset, size_t size);

		ORef<Buffer>& GetBuffer() { return m_Buffer; }
		const Buffer* GetBufferRawPtr() const { return m_Buffer.Get(); }
		Buffer* GetBufferRawPtr() { return m_Buffer.Get(); }
		void* GetBufferNativePtr() const { return m_Buffer->GetNativePtr(); }
		
	private:
		ORef<Buffer> m_Buffer;
	};
}
