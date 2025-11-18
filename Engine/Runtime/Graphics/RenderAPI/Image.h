#pragma once

#include "RenderAPITypes.h"
#include "Math/Vector3I.h"

namespace tyr
{
	struct ImageHandle : public ResourceHandle {};
	struct ImageViewHandle : public ResourceHandle {};
	struct SamplerHandle : public ResourceHandle {};

	struct ImageDesc
	{
		TYR_DECLARE_GDEBUGSTRING(debugName);
		// Handle to an externally created image required for swapchain images
		Handle externalImage = nullptr;
		uint width;
		uint height;
		uint depth;
		uint mipLevelCount;
		uint arrayLayerCount;
		ImageType type;
		PixelFormat format;
		SampleCount sampleCount;
		ImageUsage usage;
		ImageLayout layout = IMAGE_LAYOUT_GENERAL;
		MemoryProperty memoryProperty;
	};

	struct SubresourceRange
	{
		SubresourceAspect aspect;
		uint baseMipLevel;
		uint mipLevelCount;
		uint baseArrayLayer;
		uint arrayLayerCount;
	};

	struct SubresourceLayers
	{
		SubresourceAspect aspect;
		uint mipLevel;
		uint baseArrayLayer;
		uint arrayLayerCount;
	};
	
	union ClearColourValue
	{
		float float32[4];
		int	int32[4];
		uint uint32[4];
	};

	struct ClearDepthStencilValue
	{
		float depth;
		uint stencil;
	};

	union ClearValue
	{
		ClearColourValue colour;
		ClearDepthStencilValue depthStencil;
	};

	struct ImageCopyInfo
	{
		SubresourceLayers srcSubresource;
		SubresourceLayers dstSubresource;
		Vector3I srcOffset;
		Vector3I dstOffset;
		Extents3 extent;
	};

	struct BufferImageCopyInfo
	{
		uint64 bufferOffset;
		uint bufferRowLength;
		uint bufferImageHeight;
		SubresourceLayers imageSubresource;
		Vector3I imageOffset;
		Extents3 imageExtent;
	};

	struct ImageViewDesc
	{
#if !TYR_FINAL
		GDebugString debugName;
#endif
		ImageHandle image;
		SubresourceRange subresourceRange;
		ImageType viewType;
		// Specifies whether this image view points at a swap chain image
		bool isSwapChainView = false;
	};

	struct SamplerDesc
	{
		TYR_DECLARE_GDEBUGSTRING(debugName);
		Filter magFilter;
		Filter minFilter;
		SamplerMipmapMode mipmapMode;
		SamplerAddressMode	addressModeU;
		SamplerAddressMode	addressModeV;
		SamplerAddressMode	addressModeW;
		CompareOp compareOp;
		BorderColour borderColour;
		float minLod;
		float maxLod;
		float mipLodBias;
		float maxAnisotropy;
		bool anisotropyEnable;
		bool compareEnable;
		bool unnormalisedCoords;
	};
}
