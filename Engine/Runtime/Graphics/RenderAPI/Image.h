#pragma once

#include "RenderAPITypes.h"
#include "Math/Vector3I.h"

namespace tyr
{
	TYR_CREATE_HANDLE_TYPE(ImageHandle);
	TYR_CREATE_HANDLE_TYPE(ImageViewHandle);
	TYR_CREATE_HANDLE_TYPE(SamplerHandle);

	struct ImageDesc
	{
		TYR_DECLARE_GDEBUGNAME(debugName);
		// Pointer to an externally created image required for swapchain images
		void* externalImage = nullptr;
		uint width;
		uint height;
		uint depth;
		uint mipCount;
		uint arrayLayerCount;
		ImageUsage usage;
		MemoryProperty memoryProperty;
		ImageType type;
		PixelFormat format;
		SampleCount sampleCount;
		ImageLayout layout = IMAGE_LAYOUT_GENERAL;
	};

	struct SubresourceRange
	{
		SubresourceAspect aspect;
		uint baseMipLevel;
		uint mipCount;
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
		int int32[4];
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
		TYR_DECLARE_GDEBUGNAME(debugName);
		ImageHandle image;
		SubresourceRange subresourceRange;
		ImageType viewType;
		// Specifies whether this image view points at a swap chain image
		bool isSwapChainView = false;
	};

	struct SamplerDesc
	{
		TYR_DECLARE_GDEBUGNAME(debugName);
		Filter magFilter;
		Filter minFilter;
		SamplerMipmapMode mipmapMode;
		SamplerAddressMode addressModeU;
		SamplerAddressMode addressModeV;
		SamplerAddressMode addressModeW;
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
