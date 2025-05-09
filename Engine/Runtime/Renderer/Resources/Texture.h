#pragma once

#include "RenderResource.h"
#include "RenderAPI/RenderAPITypes.h"
#include "RenderAPI/Image.h"
#include "Math/Vector3I.h"

namespace tyr
{
	struct TextureInfo
	{
		uint16 width;
		uint16 height;
		// depth only used for Image3D image type
		uint16 depth;
		PixelFormat format;
		ImageType type;
		uint8 mipLevelCount;

		uint64 GetID() const
		{
			return (((uint64)width) << 48) | (((uint64)height) << 32) | (((uint64)depth) << 16) | (((uint64)format) << 8) | (((uint64)type) << 4) | ((uint64)mipLevelCount);
		}
	};

	struct TextureDesc 
	{
		AssetID assetID;
#if TYR_DEBUG
		String debugName;
#endif
		TextureInfo info;
		uint arrayLayerCount;
		ImageUsage usage;
		SampleCount sampleCount;
		ImageLayout layout;
	};

	class Device; 
	class TransferBuffer;

	/// Class repesenting a texture
	class TYR_RENDERER_EXPORT Texture final : public RenderResource 
	{
	public:
		Texture(Device& device, const TextureDesc& desc);
		~Texture() = default;

		// Should only be called when the texture is no longer in use
		void Recreate(const TextureDesc& desc);

		static Ref<Texture> Create(Device& device, const TextureDesc& desc);

		Ref<Image>& GetImage() { return m_Image; }

		Ref<ImageView>& GetImageView() { return m_ImageView; }

		ImageLayout GetImageLayout() const { return m_ImageLayout; }

		// Returns the size of the image in bytes
		uint GetImageSize() const { return m_ImageSize; }

		bool IsCubemap() const { return m_IsCubemap; }

		bool IsDepthTexture() const { return m_IsDepthTexture; }

		// Returns the pixel size for the specified format in bytes
		static uint GetTexelFormatSize(PixelFormat format);

	private:
		Ref<Image> m_Image;
		Ref<ImageView> m_ImageView;
		// Current layout tracked by the render graph 
		friend class RenderGraph;
		ImageLayout m_ImageLayout;
		uint m_ImageSize;

		bool m_IsCubemap;
		bool m_IsDepthTexture;
	};
}

namespace std
{
	template <>
	struct hash<tyr::TextureInfo>
	{
		size_t operator()(const tyr::TextureInfo& id) const noexcept
		{
			return std::hash<tyr::uint64>{}(id.GetID());
		}
	};
}
