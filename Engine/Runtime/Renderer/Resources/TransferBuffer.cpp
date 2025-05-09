#include "TransferBuffer.h"
#include "Core.h"
#include "RenderAPI/Device.h"
#include "RenderAPI/Sync.h"
#include "RenderAPI/CommandList.h"

namespace tyr
{
	TransferBuffer::TransferBuffer(Device& device, const TransferBufferDesc& desc)
		: RenderResource(device)
	{
		Recreate(desc);
	}

	TransferBuffer::~TransferBuffer()
	{

	}

	void TransferBuffer::Recreate(const TransferBufferDesc& desc)
	{
		BufferDesc bufferDesc;
#if TYR_DEBUG 
		bufferDesc.debugName = String(desc.debugName) + "_TransferBuffer";
#endif
		bufferDesc.usage = BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferDesc.memoryProperty = static_cast<MemoryProperty>(MEMORY_PROPERTY_HOST_VISIBLE_BIT | MEMORY_PROPERTY_HOST_COHERENT_BIT);
		bufferDesc.size = desc.size;

		m_Buffer = m_Device.CreateBuffer(bufferDesc);
	}

	Ref<TransferBuffer> TransferBuffer::Create(Device& device, const TransferBufferDesc& desc)
	{
		return Ref<TransferBuffer>(new TransferBuffer(device, desc));
	}

	void TransferBuffer::CreateTransferBarrier(BufferBarrier& barrier, uint srcQueueFamilyIndex)
	{
		barrier.buffer = m_Buffer.get();
		barrier.srcAccess = BARRIER_ACCESS_NONE;
		barrier.dstAccess = BARRIER_ACCESS_TRANSFER_READ_BIT;
		barrier.srcStage = PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		barrier.dstStage = PIPELINE_STAGE_TRANSFER_BIT;
		barrier.srcQueueFamilyIndex = srcQueueFamilyIndex;
	}

	void TransferBuffer::Write(const void* data, size_t offset, size_t size)
	{	
		m_Buffer->Write(data, offset, size);	
	}
}