#include "TextureUtil.h"
#include "RenderAPI/Buffer.h"
#include "RenderAPI/Device.h"
#include "Core.h"

namespace tyr
{
	uint TextureUtil::CalculateTexelFormatSize(PixelFormat format)
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