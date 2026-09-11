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
		for (uint i = 0; i < count; ++i)
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
		}
	}

    void CreateImageCopyInfos(BufferImageCopyInfo* copyInfos, uint highestMip, uint mipCount, const TextureInfo& textureInfo)
    {
        // Currently just supporting BC3 / BC5 / BC7 
        const uint blockW = TextureUtil::GetTextureBlockWidth(textureInfo.format);
        const uint blockH = TextureUtil::GetTextureBlockHeight(textureInfo.format);
        const uint bytesPerBlock = TextureUtil::GetTextureBlockSize(textureInfo.format);

        size_t currentOffset = 0;
        uint copyIndex = 0;

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
                BufferImageCopyInfo& info = copyInfos[copyIndex++];

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
		LocalArray<BufferImageCopyInfo, RenderConstants::c_MaxMips> copyInfos;
		for (uint i = 0; i < count; ++i)
		{
            Texture& dstTexture = registry.GetTexture(requests[i].dstTexture);
			const TextureUploadRequest& request = requests[i];
			CreateImageCopyInfos(copyInfos.Data(), request.highestMip, request.mipCount, dstTexture.info);
			commandList.CopyBufferToImage(registry.GetBuffer(requests[i].srcBuffer).buffer, dstTexture.image, dstTexture.imageLayout, copyInfos.Data(), copyInfos.Size());
			copyInfos.Clear();
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