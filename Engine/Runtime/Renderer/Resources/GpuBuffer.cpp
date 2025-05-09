#include "GpuBuffer.h"
#include "Core.h"
#include "RenderAPI/Device.h"
#include "RenderAPI/Sync.h"
#include "RenderAPI/CommandList.h"
#include "TransferBuffer.h"

namespace tyr
{
	GpuBuffer::GpuBuffer(Device& device, const GpuBufferDesc& desc)
		: RenderResource(device)
	{
		Recreate(desc);
	}

	GpuBuffer::~GpuBuffer()
	{

	}

	void GpuBuffer::Recreate(const GpuBufferDesc& desc)
	{
		m_UseTransferBuffer = desc.useTransferBuffer;
		if (desc.useTransferBuffer)
		{
			CreateTransferBuffer(desc);
		}
		CreateBufferAndView(desc);
	}

	Ref<GpuBuffer> GpuBuffer::Create(Device& device, const GpuBufferDesc& desc)
	{
		return Ref<GpuBuffer>(new GpuBuffer(device, desc));
	}

	void GpuBuffer::CreateTransferBarriers(BufferBarrier& transferBarrier, BufferBarrier& gpuBarrier, uint srcQueueFamilyIndex)
	{
		m_TransferBuffer->CreateTransferBarrier(transferBarrier, srcQueueFamilyIndex);
		CreateTransferBarrier(gpuBarrier, srcQueueFamilyIndex);
	}

	void GpuBuffer::CreateTransferBarrier(BufferBarrier& gpuBarrier, uint srcQueueFamilyIndex)
	{
		gpuBarrier.buffer = m_Buffer.get();
		gpuBarrier.srcAccess = BARRIER_ACCESS_NONE;
		gpuBarrier.dstAccess = BARRIER_ACCESS_TRANSFER_WRITE_BIT;
		gpuBarrier.srcStage = PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		gpuBarrier.dstStage = PIPELINE_STAGE_TRANSFER_BIT;
		gpuBarrier.srcQueueFamilyIndex = srcQueueFamilyIndex;
	}

	void GpuBuffer::CreateTransferBuffer(const GpuBufferDesc& desc)
	{
		TransferBufferDesc bufferDesc;
#if TYR_DEBUG 
		bufferDesc.debugName = String(desc.debugName) + "_TransferBuffer";
#endif
		bufferDesc.size = desc.size;

		m_TransferBuffer = TransferBuffer::Create(m_Device, bufferDesc);
	}

	void GpuBuffer::CreateBufferAndView(const GpuBufferDesc& desc)
	{
		BufferDesc bufferDesc;
#if TYR_DEBUG 
		String debugName = String(desc.debugName) + "_GpuBuffer";
		bufferDesc.debugName = debugName.c_str();
#endif
		// TODO: Add raytracing support.
		switch (desc.usage)
		{
		case GpuBufferUsage::UniformTexel:
			bufferDesc.usage = static_cast<BufferUsage>(BUFFER_USAGE_TRANSFER_DST_BIT | BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT);
			break;
		case GpuBufferUsage::StorageTexel:
			bufferDesc.usage = static_cast<BufferUsage>(BUFFER_USAGE_TRANSFER_DST_BIT | BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT);
			break;
		case GpuBufferUsage::Uniform:
			bufferDesc.usage = static_cast<BufferUsage>(BUFFER_USAGE_TRANSFER_DST_BIT | BUFFER_USAGE_UNIFORM_BUFFER_BIT);
			break;
		case GpuBufferUsage::Storage:
			bufferDesc.usage = static_cast<BufferUsage>(BUFFER_USAGE_TRANSFER_DST_BIT | BUFFER_USAGE_STORAGE_BUFFER_BIT);
			break;
		case GpuBufferUsage::Index:
			bufferDesc.usage = static_cast<BufferUsage>(BUFFER_USAGE_TRANSFER_DST_BIT | BUFFER_USAGE_INDEX_BUFFER_BIT);
			break;
		case GpuBufferUsage::Vertex:
			bufferDesc.usage = static_cast<BufferUsage>(BUFFER_USAGE_TRANSFER_DST_BIT | BUFFER_USAGE_VERTEX_BUFFER_BIT);
			break;
		case GpuBufferUsage::IndexAndVertex:
			bufferDesc.usage = static_cast<BufferUsage>(BUFFER_USAGE_TRANSFER_DST_BIT | BUFFER_USAGE_INDEX_BUFFER_BIT
				| BUFFER_USAGE_VERTEX_BUFFER_BIT);
			break;
		default:
			TYR_ASSERT(false);
		}

		if (m_Device.IsDiscreteGPU())
		{
			bufferDesc.memoryProperty = MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		}
		else
		{
			bufferDesc.memoryProperty = static_cast<MemoryProperty>(MEMORY_PROPERTY_HOST_VISIBLE_BIT | MEMORY_PROPERTY_HOST_COHERENT_BIT);
		}
		bufferDesc.size = desc.size;
		bufferDesc.stride = desc.stride;

		m_Buffer = m_Device.CreateBuffer(bufferDesc);

		BufferViewDesc viewDesc;
		viewDesc.buffer = m_Buffer;
		viewDesc.offset = 0;
		viewDesc.size = bufferDesc.size;
		m_BufferView = m_Device.CreateBufferView(viewDesc);
	}

	void GpuBuffer::Write(const void* data, size_t offset, size_t size)
	{
		if (m_UseTransferBuffer)
		{
			m_TransferBuffer->Write(data, offset, size);
		}
	}

	void GpuBuffer::Upload(Ref<CommandList>& commandList)
	{
		if (m_UseTransferBuffer)
		{
			commandList->CopyBuffer(m_TransferBuffer->GetBuffer(), m_Buffer);
		}
	}
}