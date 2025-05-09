
#pragma once

#include "RenderAPITypes.h"
#include "Math/Vector3I.h"

namespace tyr
{
	struct ImageDesc
	{
#if TYR_DEBUG
		String debugName;
#endif
		uint width;
		uint height;
		uint depth;
		uint mipLevelCount;
		uint arrayLayerCount;
		ImageType type;
		PixelFormat format;
		SampleCount sampleCount;
		ImageUsage usage;
		ImageLayout layout;
		MemoryProperty memoryProperty;
		// Handle to an externally created image required for swapchain images
		Handle externalImage = nullptr;
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
		uint32_t stencil;
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

	class Device; 
	class Buffer;

	/// Class repesenting a image
	class TYR_GRAPHICS_EXPORT Image
	{
	public:
		Image(Device& device, const ImageDesc& desc);
		virtual ~Image() = default;

		const ImageDesc& GetDesc() const { return m_Desc; }

		// Returns the size allocated for the image in bytes.
		// Should not be called for externally created images (ie. swapchain images).
		size_t GetSizeAllocated() const 
		{ 
			TYR_ASSERT(!m_Desc.externalImage);
			return m_SizeAllocated; 
		};

	protected:
		Device& m_Device;
		ImageDesc m_Desc;
		size_t m_SizeAllocated;
	};

	struct ImageViewDesc
	{
#if TYR_DEBUG
		String debugName;
#endif
		Ref<Image> image;
		SubresourceRange subresourceRange;
		ImageType viewType;
		// Specifies whether this image view points at a swap chain image
		bool isSwapChainView = false;
	};

	/// Class repesenting a image view
	class TYR_GRAPHICS_EXPORT ImageView
	{
	public:
		ImageView(const ImageViewDesc& desc);
		virtual ~ImageView() = default;

		const ImageViewDesc& GetDesc() const { return m_Desc; }
	protected:
		ImageViewDesc m_Desc;

	};

	struct SamplerDesc
	{
		String debugName;
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

	/// Class repesenting a image sampler
	class TYR_GRAPHICS_EXPORT Sampler
	{
	public:
		Sampler(const SamplerDesc& desc);
		virtual ~Sampler() = default;

		const SamplerDesc& GetDesc() const { return m_Desc; }
	protected:
		SamplerDesc m_Desc;

	};
}
