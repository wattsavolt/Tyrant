#include "Texture.h"
#include "RenderAPI/Buffer.h"
#include "RenderAPI/Device.h"
#include "Core.h"

namespace tyr
{
	TYR_REFL_CLASS_START(PixelFormat, 0);
	TYR_REFL_CLASS_END();

	TYR_REFL_CLASS_START(ImageType, 0);
	TYR_REFL_CLASS_END();

	TYR_REFL_CLASS_START(TextureInfo, 0);
		TYR_REFL_FIELD(&TextureInfo::width, "Width", true, true, true);
		TYR_REFL_FIELD(&TextureInfo::height, "Height", true, true, true);
		TYR_REFL_FIELD(&TextureInfo::depth, "Depth", true, true, true);
		TYR_REFL_FIELD(&TextureInfo::format, "Format", true, true, true);
		TYR_REFL_FIELD(&TextureInfo::type, "Type", true, true, true);
		TYR_REFL_FIELD(&TextureInfo::mipLevelCount, "MipCount", true, true, true);
	TYR_REFL_CLASS_END();

	void TextureUtil::CreateTexture(Texture& texture, Device& device, const TextureDesc& desc)
	{
		const bool isCubemap = desc.info.type == ImageType::Cubemap || desc.info.type == ImageType::CubemapArray;
		TYR_ASSERT(!isCubemap || desc.arrayLayerCount % 6 == 0);

		ImageDesc imageDesc;
#if !TYR_FINAL
		imageDesc.debugName = desc.debugName;
#endif
		imageDesc.type = desc.info.type;
		imageDesc.format = desc.info.format;
		imageDesc.width = static_cast<uint>(desc.info.width);
		imageDesc.height = static_cast<uint>(desc.info.height);
		imageDesc.depth = static_cast<uint>(desc.info.depth);
		imageDesc.mipLevelCount = static_cast<uint>(desc.info.mipLevelCount);
		imageDesc.arrayLayerCount = desc.arrayLayerCount;
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
		viewDesc.debugName.Set(desc.debugName.CStr(), "_View");
#endif
		viewDesc.image = texture.image;
		viewDesc.isSwapChainView = false;
		viewDesc.viewType = desc.info.type;
		viewDesc.subresourceRange.aspect = isDepthTexture ? SUBRESOURCE_ASPECT_DEPTH_BIT : SUBRESOURCE_ASPECT_COLOUR_BIT;
		viewDesc.subresourceRange.baseMipLevel = 0;
		viewDesc.subresourceRange.mipLevelCount = desc.info.mipLevelCount;
		viewDesc.subresourceRange.baseArrayLayer = 0;
		viewDesc.subresourceRange.arrayLayerCount = desc.arrayLayerCount;

		texture.imageView = device.CreateImageView(viewDesc);

		texture.sampler = desc.sampler;
	}
	
	void TextureUtil::DeleteTexture(Texture& texture, Device& device)
	{
		device.DeleteImageView(texture.imageView);
		device.DeleteImage(texture.image);
	}

	uint TextureUtil::GetTexelFormatSize(PixelFormat format)
	{
		switch (format)
		{
		case PF_D16_UNORM:
			return 2;

		case PF_R8G8B8A8_UNORM:
		case PF_R8G8B8A8_SNORM:
		case PF_R8G8B8A8_SSCALED:
		case PF_R8G8B8A8_USCALED:
		case PF_R8G8B8A8_SINT:
		case PF_R8G8B8A8_UINT:
		case PF_R8G8B8A8_SRGB:

		case PF_B8G8R8A8_UNORM:
		case PF_B8G8R8A8_SNORM:
		case PF_B8G8R8A8_SSCALED:
		case PF_B8G8R8A8_USCALED:
		case PF_B8G8R8A8_SINT:
		case PF_B8G8R8A8_UINT:
		case PF_B8G8R8A8_SRGB:

		case PF_D24_UNORM_S8_UINT:
		case PF_D32_SFLOAT:
			return 4;

		case PF_D32_FLOAT_S8_UINT:
			return 5;

		case PF_R16G16B16A16_UNORM:
		case PF_R16G16B16A16_SNORM:
		case PF_R16G16B16A16_SSCALED:
		case PF_R16G16B16A16_USCALED:
		case PF_R16G16B16A16_SINT:
		case PF_R16G16B16A16_UINT:
		case PF_R16G16B16A16_SFLOAT:

		case PF_B16G16R16A16_UNORM:
		case PF_B16G16R16A16_SNORM:
		case PF_B16G16R16A16_SSCALED:
		case PF_B16G16R16A16_USCALED:
		case PF_B16G16R16A16_SINT:
		case PF_B16G16R16A16_UINT:
		case PF_B16G16R16A16_SFLOAT:
			return 8;

		case PF_R32G32B32_UINT:
		case PF_R32G32B32_SINT:
		case PF_R32G32B32_SFLOAT:
			return 12;

		case PF_R32G32B32A32_SINT:
		case PF_R32G32B32A32_UINT:
		case PF_R32G32B32A32_SFLOAT:
			return 16;
		}

		TYR_ASSERT(false);
		return 0;
	}

	float TextureUtil::SRGBToLinear(float srgb)
	{
		if (srgb <= 0.04045f)
		{
			return srgb / 12.92f;
		}
		return powf((srgb + 0.055f) / 1.055f, 2.4f);
	}

	float TextureUtil::LinearToSRGB(float linear)
	{
		if (linear <= 0.0031308f)
		{
			return linear * 12.92f;
		}
		return 1.055f * powf(linear, 1.0f / 2.4f) - 0.055f;
	}
}