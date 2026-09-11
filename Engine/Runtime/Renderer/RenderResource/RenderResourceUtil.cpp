#include "RenderResourceUtil.h"
#include "Core.h"
#include "RenderAPI/Device.h"
#include "RenderAPI/CommandList.h"
#include "RenderResource/RenderBuffer.h"
#include "RenderResource/Texture.h"

namespace tyr
{
	void RenderResourceUtil::InitializeRenderBuffer(RenderBuffer& buffer, Device& device, const RenderBufferDesc& desc)
	{
		buffer.renderGraphIndex = c_RenderGraphInvalidIndex;

		buffer.type = RenderResourceType::Buffer;

		BufferDesc bufferDesc;
#if !TYR_FINAL 
		bufferDesc.debugName = desc.debugName;
#endif

		// TODO: Add raytracing support.
		switch (desc.usage)
		{
		case RenderBufferUsage::Upload:
		{
			bufferDesc.usage = BUFFER_USAGE_TRANSFER_SRC_BIT;
			break;
		}
		case RenderBufferUsage::UniformTexel:
			bufferDesc.usage = static_cast<BufferUsage>(BUFFER_USAGE_TRANSFER_DST_BIT | BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT);
			break;
		case RenderBufferUsage::StorageTexel:
			bufferDesc.usage = static_cast<BufferUsage>(BUFFER_USAGE_TRANSFER_DST_BIT | BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT);
			break;
		case RenderBufferUsage::Uniform:
			bufferDesc.usage = static_cast<BufferUsage>(BUFFER_USAGE_TRANSFER_DST_BIT | BUFFER_USAGE_UNIFORM_BUFFER_BIT);
			break;
		case RenderBufferUsage::Storage:
			bufferDesc.usage = static_cast<BufferUsage>(BUFFER_USAGE_TRANSFER_DST_BIT | BUFFER_USAGE_STORAGE_BUFFER_BIT);
			break;
		case RenderBufferUsage::Index:
			bufferDesc.usage = static_cast<BufferUsage>(BUFFER_USAGE_TRANSFER_DST_BIT | BUFFER_USAGE_INDEX_BUFFER_BIT);
			break;
		case RenderBufferUsage::Vertex:
			bufferDesc.usage = static_cast<BufferUsage>(BUFFER_USAGE_TRANSFER_DST_BIT | BUFFER_USAGE_VERTEX_BUFFER_BIT);
			break;
		case RenderBufferUsage::IndexAndVertex:
			bufferDesc.usage = static_cast<BufferUsage>(BUFFER_USAGE_TRANSFER_DST_BIT | BUFFER_USAGE_INDEX_BUFFER_BIT
				| BUFFER_USAGE_VERTEX_BUFFER_BIT);
			break;
		default:
			TYR_ASSERT(false);
		}

		if (desc.usage != RenderBufferUsage::Upload && device.IsDiscreteGPU())
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

		if (desc.usage != RenderBufferUsage::Upload)
		{
			BufferViewDesc viewDesc;
#if !TYR_FINAL 
			{
				GDebugString debugName;
				debugName.Set(desc.debugName, "_View");
				viewDesc.debugName = debugName.Str();
			}
#endif
			viewDesc.buffer = buffer.buffer;
			viewDesc.offset = 0;
			viewDesc.size = bufferDesc.size;
			buffer.bufferView = device.CreateBufferView(viewDesc);

			buffer.mappedMemory = nullptr;
		}
		else
		{
			buffer.mappedMemory = device.MapBuffer(buffer.buffer);
		}
	}

	// It's expected that this render buffer will be destructed after this function call
	void RenderResourceUtil::DeinitializeRenderBuffer(RenderBuffer& buffer, Device& device)
	{
		if (buffer.mappedMemory)
		{
			device.UnmapBuffer(buffer.buffer);
		}
		if (buffer.bufferView)
		{
			device.DeleteBufferView(buffer.bufferView);
		}
		device.DeleteBuffer(buffer.buffer);
	}

	void RenderResourceUtil::WriteUploadBuffer(RenderBuffer& buffer, Device& device, size_t offset, void* data, size_t dataSize)
	{
		memcpy(&static_cast<uint8*>(buffer.mappedMemory)[offset], data, dataSize);
		device.FlushBufferAllocation(buffer.buffer, offset, dataSize);
	}

	void RenderResourceUtil::CreateTransferWriteBarrier(BufferBarrier& barrier, BufferHandle buffer, size_t size, uint srcQueueFamilyIndex)
	{
		barrier.buffer = buffer;
		barrier.srcAccess = BARRIER_ACCESS_NONE;
		barrier.dstAccess = BARRIER_ACCESS_TRANSFER_WRITE_BIT;
		barrier.srcStage = PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		barrier.dstStage = PIPELINE_STAGE_TRANSFER_BIT;
		barrier.srcQueueFamilyIndex = srcQueueFamilyIndex; 
		barrier.size = size;
	}

	void RenderResourceUtil::CreateTransferReadBarrier(BufferBarrier& barrier, BufferHandle buffer, size_t size, uint srcQueueFamilyIndex)
	{
		barrier.buffer = buffer;
		barrier.srcAccess = BARRIER_ACCESS_NONE;
		barrier.dstAccess = BARRIER_ACCESS_TRANSFER_READ_BIT;
		barrier.srcStage = PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		barrier.dstStage = PIPELINE_STAGE_TRANSFER_BIT;
		barrier.srcQueueFamilyIndex = srcQueueFamilyIndex;
		barrier.size = size;
	}

	void RenderResourceUtil::InitializeTexture(Texture& texture, Device& device, const TextureDesc& desc)
	{
		texture.renderGraphIndex = c_RenderGraphInvalidIndex;

		texture.type = RenderResourceType::Texture;

		const bool isCubemap = desc.info.type == ImageType::Cubemap || desc.info.type == ImageType::CubemapArray;
		TYR_ASSERT(!isCubemap || desc.info.arrayLayerCount % 6 == 0);

		ImageDesc imageDesc;
#if !TYR_FINAL
		imageDesc.debugName = desc.debugName;
#endif
		imageDesc.type = desc.info.type;
		imageDesc.format = desc.info.format;
		imageDesc.width = static_cast<uint>(desc.info.width);
		imageDesc.height = static_cast<uint>(desc.info.height);
		imageDesc.depth = static_cast<uint>(desc.info.depth);
		imageDesc.mipCount = static_cast<uint>(desc.info.mipCount);
		imageDesc.arrayLayerCount = desc.info.arrayLayerCount;
		imageDesc.sampleCount = desc.sampleCount;
		imageDesc.usage = desc.usage;
		imageDesc.layout = desc.layout;
		imageDesc.memoryProperty = MemoryProperty::MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		texture.image = device.CreateImage(imageDesc);

		texture.imageLayout = desc.layout;

		const bool isDepthTexture = desc.info.format == PF_D16_UNORM || desc.info.format == PF_D24_UNORM_S8_UINT
			|| desc.info.format == PF_D32_SFLOAT || desc.info.format == PF_D32_FLOAT_S8_UINT;

		ImageViewDesc viewDesc;
#if !TYR_FINAL
		{
			GDebugString debugName;
			debugName.Set(desc.debugName, "_View");
			viewDesc.debugName = debugName.Str();
		}
#endif
		viewDesc.image = texture.image;
		viewDesc.isSwapChainView = false;
		viewDesc.viewType = desc.info.type;
		viewDesc.subresourceRange.aspect = isDepthTexture ? SUBRESOURCE_ASPECT_DEPTH_BIT : SUBRESOURCE_ASPECT_COLOUR_BIT;
		viewDesc.subresourceRange.baseMipLevel = 0;
		viewDesc.subresourceRange.mipCount = desc.info.mipCount;
		viewDesc.subresourceRange.baseArrayLayer = 0;
		viewDesc.subresourceRange.arrayLayerCount = desc.info.arrayLayerCount;

		texture.imageView = device.CreateImageView(viewDesc);

		texture.sampler = desc.sampler;

		texture.info = desc.info;
	}

	void RenderResourceUtil::DeinitializeTexture(Texture& texture, Device& device)
	{
		device.DeleteImageView(texture.imageView);
		device.DeleteImage(texture.image);
	}
}