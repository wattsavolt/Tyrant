#include "GpuTransferUtil.h"
#include "UploadRequest.h"
#include "RenderAPI/CommandList.h"
#include "Rendering/RenderConstants.h"
#include "Rendering/RenderRegistry.h"

namespace tyr
{
	void GpuTransferUtil::UploadToBuffers(CommandList& commandList, BufferUploadRequest* requests, uint count)
	{
        RenderRegistry& registry = *RenderRegistry::Instance();
		std::sort(requests, requests + count);
		LocalArray<BufferCopyInfo, c_MaxUploadRegionsPerBuffer> copyInfos;
		for (uint i = 0; i < count; )
		{
			uint j = i;
			while (j < count && requests[i].SameResources(requests[j]))
			{
				BufferUploadRequest& request = requests[j];
				BufferCopyInfo& info = copyInfos.ExpandOne();
				info.srcOffset = request.srcOffset;
				info.dstOffset = request.dstOffset;
				info.size = request.size;
				++j;
			}

			commandList.CopyBuffer(registry.GetBuffer(requests[i].srcBuffer).buffer, registry.GetBuffer(requests[i].dstBuffer).buffer, copyInfos.Data(), copyInfos.Size());

			copyInfos.Clear();
			// Advance past the whole group just batched, not just by one - otherwise every
			// request after the first in a group gets re-batched (and re-copied) again as its
			// own group start too, eventually overrunning copyInfos' fixed capacity.
			i = j;
		}
	}

    void CreateImageCopyInfos(LocalArray<BufferImageCopyInfo, RenderConstants::c_MaxMips>& copyInfos, uint highestMip, uint mipCount, const TextureInfo& textureInfo, size_t baseOffset)
    {
        // Currently just supporting BC3 / BC5 / BC7
        const uint blockW = TextureUtil::GetTextureBlockWidth(textureInfo.format);
        const uint blockH = TextureUtil::GetTextureBlockHeight(textureInfo.format);
        const uint bytesPerBlock = TextureUtil::GetTextureBlockSize(textureInfo.format);

        size_t currentOffset = baseOffset;

        for (uint i = 0; i < mipCount; ++i)
        {
            const uint mip = highestMip + i;

            const uint width = std::max(1u, static_cast<uint>(textureInfo.width) >> mip);
            const uint height = std::max(1u, static_cast<uint>(textureInfo.height) >> mip);

            // Block compressed math
            const uint blocksX = (width + blockW - 1) / blockW;
            const uint blocksY = (height + blockH - 1) / blockH;

            uint rowSize = blocksX * bytesPerBlock;
            rowSize = MemoryUtil::Align(rowSize, RenderConstants::c_RowPitchAlignment);

            uint layerSize = rowSize * blocksY;
            layerSize = MemoryUtil::Align(layerSize, RenderConstants::c_UploadAlignment);

            for (uint layer = 0; layer < textureInfo.arrayLayerCount; ++layer)
            {
                BufferImageCopyInfo& info = copyInfos.ExpandOne();

                info.bufferOffset = static_cast<uint64>(MemoryUtil::Align(currentOffset, static_cast<size_t>(RenderConstants::c_UploadAlignment)));
                info.bufferRowLength = 0;     
                info.bufferImageHeight = 0;

                info.imageSubresource.aspect = SUBRESOURCE_ASPECT_COLOUR_BIT;
                info.imageSubresource.mipLevel = mip;
                info.imageSubresource.baseArrayLayer = layer;
                info.imageSubresource.arrayLayerCount = 1;

                info.imageOffset = { 0, 0, 0 };

                info.imageExtent.width = width;
                info.imageExtent.height = height;
                info.imageExtent.depth = 1;

                currentOffset = info.bufferOffset + layerSize;
            }
        }
    }

	void GpuTransferUtil::UploadToTextures(CommandList& commandList, const TextureUploadRequest* requests, uint count)
	{
        RenderRegistry& registry = *RenderRegistry::Instance();

		// Every one of these textures was just created (Device::CreateImage can only legally
		// set VkImageCreateInfo::initialLayout to UNDEFINED, never straight to the texture's
		// intended steady-state layout - see its own comment), so each one still needs an
		// actual transition out of UNDEFINED before it can be a valid copy destination. This
		// assumes each texture only ever goes through UploadToTextures once, right after
		// creation - a genuine re-upload later would need to preserve existing content instead
		// of transitioning from UNDEFINED (which permits the driver to discard it).
		{
			LocalArray<ImageBarrier, RenderConstants::c_MaxTextures> initialBarriers;
			for (uint i = 0; i < count; ++i)
			{
				const Texture& dstTexture = registry.GetTexture(requests[i].dstTexture);

				ImageBarrier& barrier = initialBarriers.ExpandOne();
				barrier.image = dstTexture.image;
				barrier.srcAccess = BARRIER_ACCESS_NONE;
				barrier.dstAccess = BARRIER_ACCESS_TRANSFER_WRITE_BIT;
				barrier.srcLayout = IMAGE_LAYOUT_UNKNOWN;
				barrier.dstLayout = dstTexture.imageLayout;
				barrier.srcStage = PIPELINE_STAGE_TOP_OF_PIPE_BIT;
				barrier.dstStage = PIPELINE_STAGE_TRANSFER_BIT;
				barrier.subresourceRange.aspect = SUBRESOURCE_ASPECT_COLOUR_BIT;
				barrier.subresourceRange.baseMipLevel = 0;
				barrier.subresourceRange.mipCount = dstTexture.info.mipCount;
				barrier.subresourceRange.baseArrayLayer = 0;
				barrier.subresourceRange.arrayLayerCount = dstTexture.info.arrayLayerCount;
			}

			if (!initialBarriers.IsEmpty())
			{
				commandList.AddBarriers(nullptr, 0, initialBarriers.Data(), initialBarriers.Size());
			}
		}

		LocalArray<BufferImageCopyInfo, RenderConstants::c_MaxMips> copyInfos;
		for (uint i = 0; i < count; ++i)
		{
            Texture& dstTexture = registry.GetTexture(requests[i].dstTexture);
			const TextureUploadRequest& request = requests[i];
			CreateImageCopyInfos(copyInfos, request.highestMip, request.mipCount, dstTexture.info, request.srcOffset);
			commandList.CopyBufferToImage(registry.GetBuffer(requests[i].srcBuffer).buffer, dstTexture.image, dstTexture.imageLayout, copyInfos.Data(), copyInfos.Size());
			copyInfos.Clear();
		}

		// Unlike the shared buffers (mesh/vertex/index/etc, tracked and barriered by the render
		// graph - see Renderer::BuildAndExecuteRenderGraph), individual textures aren't
		// registered with it at all, so nothing else ever synchronizes this copy against
		// MeshPS.hlsl's later Sample() of the same image within the same command buffer. Without
		// this, the two have no ordering guarantee - the shader read is free to happen before the
		// copy's writes are visible, which reads back as all-zero/black regardless of how correct
		// the actual texture data is.
		if (count > 0)
		{
			LocalArray<ImageBarrier, RenderConstants::c_MaxTextures> barriers;
			for (uint i = 0; i < count; ++i)
			{
				const Texture& dstTexture = registry.GetTexture(requests[i].dstTexture);

				ImageBarrier& barrier = barriers.ExpandOne();
				barrier.image = dstTexture.image;
				barrier.srcAccess = BARRIER_ACCESS_TRANSFER_WRITE_BIT;
				barrier.dstAccess = BARRIER_ACCESS_SHADER_READ_BIT;
				barrier.srcLayout = dstTexture.imageLayout;
				barrier.dstLayout = dstTexture.imageLayout;
				barrier.srcStage = PIPELINE_STAGE_TRANSFER_BIT;
				barrier.dstStage = PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				barrier.subresourceRange.aspect = SUBRESOURCE_ASPECT_COLOUR_BIT;
				barrier.subresourceRange.baseMipLevel = 0;
				barrier.subresourceRange.mipCount = dstTexture.info.mipCount;
				barrier.subresourceRange.baseArrayLayer = 0;
				barrier.subresourceRange.arrayLayerCount = dstTexture.info.arrayLayerCount;
			}

			commandList.AddBarriers(nullptr, 0, barriers.Data(), barriers.Size());
		}
	}

	void GpuTransferUtil::FileToBuffer(CommandList& commandList, const FileToBufferUploadRequest& request)
	{
        // TODO : Implement
	}

	void GpuTransferUtil::FileToTexture(CommandList& commandList, const FileToTextureUploadRequest& request)
	{
        // TODO : Implement
	}
}