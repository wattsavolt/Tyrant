#include "RenderBuffer.h"
#include "Core.h"
#include "RenderAPI/Device.h"
#include "RenderAPI/CommandList.h"

namespace tyr
{
	void RenderBufferUtil::CreateTransferBuffer(RenderBuffer& buffer, Device& device, const TransferBufferDesc& desc)
	{
		BufferDesc bufferDesc;
#if !TYR_FINAL 
		bufferDesc.debugName = desc.debugName;
#endif
		bufferDesc.usage = BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferDesc.memoryProperty = static_cast<MemoryProperty>(MEMORY_PROPERTY_HOST_VISIBLE_BIT | MEMORY_PROPERTY_HOST_COHERENT_BIT);
		bufferDesc.size = desc.size;

		buffer.buffer = device.CreateBuffer(bufferDesc);
	}

	void RenderBufferUtil::CreateGpuBuffer(RenderBuffer& buffer, Device& device, const GpuBufferDesc& desc)
	{
		BufferDesc bufferDesc;
#if !TYR_FINAL 
		bufferDesc.debugName = desc.debugName;
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

		if (device.IsDiscreteGPU())
		{
			bufferDesc.memoryProperty = MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		}
		else
		{
			bufferDesc.memoryProperty = static_cast<MemoryProperty>(MEMORY_PROPERTY_HOST_VISIBLE_BIT | MEMORY_PROPERTY_HOST_COHERENT_BIT);
		}
		bufferDesc.size = desc.size;
		bufferDesc.stride = desc.stride;

		buffer.buffer = device.CreateBuffer(bufferDesc);

		BufferViewDesc viewDesc;
#if !TYR_FINAL 
		viewDesc.debugName.Set(desc.debugName.CStr(), "_View");
#endif
		viewDesc.buffer = buffer.buffer;
		viewDesc.offset = 0;
		viewDesc.size = bufferDesc.size;
		buffer.bufferView = device.CreateBufferView(viewDesc);
	}

	void RenderBufferUtil::DeleteBuffer(RenderBuffer& buffer, Device& device)
	{
		if (buffer.bufferView)
		{
			device.DeleteBufferView(buffer.bufferView);
		}
		device.DeleteBuffer(buffer.buffer);
	}

	void RenderBufferUtil::CreateTransferWriteBarrier(BufferBarrier& barrier, BufferHandle buffer, size_t size, uint srcQueueFamilyIndex)
	{
		barrier.buffer = buffer;
		barrier.srcAccess = BARRIER_ACCESS_NONE;
		barrier.dstAccess = BARRIER_ACCESS_TRANSFER_WRITE_BIT;
		barrier.srcStage = PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		barrier.dstStage = PIPELINE_STAGE_TRANSFER_BIT;
		barrier.srcQueueFamilyIndex = srcQueueFamilyIndex; 
		barrier.size = size;
	}

	void RenderBufferUtil::CreateTransferReadBarrier(BufferBarrier& barrier, BufferHandle buffer, size_t size, uint srcQueueFamilyIndex)
	{
		barrier.buffer = buffer;
		barrier.srcAccess = BARRIER_ACCESS_NONE;
		barrier.dstAccess = BARRIER_ACCESS_TRANSFER_READ_BIT;
		barrier.srcStage = PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		barrier.dstStage = PIPELINE_STAGE_TRANSFER_BIT;
		barrier.srcQueueFamilyIndex = srcQueueFamilyIndex;
		barrier.size = size;
	}
}