#pragma once

#include "RenderAPI/RenderAPITypes.h"
#include "RenderAPI/Image.h"
#include "Math/Vector3I.h"
#include "RenderResource.h"

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
#if !TYR_FINAL
		GDebugString debugName;
#endif
		SamplerHandle sampler;
		uint arrayLayerCount;
		ImageUsage usage;
		TextureInfo info;
		SampleCount sampleCount;
		ImageLayout layout;
	};

	class Device; 

	struct Texture : public RenderResource
	{
		ImageHandle image;
		ImageViewHandle imageView;
		SamplerHandle sampler;
		ImageLayout imageLayout;
	};

	class TYR_RENDERER_EXPORT TextureUtil
	{
	public:
		static void CreateTexture(Texture& texture, Device& device, const TextureDesc& desc);

		static void DeleteTexture(Texture& texture, Device& device);

		// Returns the pixel size for the specified format in bytes
		static uint GetTexelFormatSize(PixelFormat format);

		static float SRGBToLinear(float f);

		static float LinearToSRGB(float f);

		template <typename T>
		static float ConvertFromSRGBtoLinear(T value)
		{
			if constexpr (std::is_same_v<T, uint8_t>)
			{
				constexpr float oneOver255 = 1.0f / 255.0f;
				float f = value * oneOver255;
				return SRGBToLinear(f);
			}
			else if constexpr (std::is_same_v<T, float>)
			{
				return SRGBToLinear(value);
			}
			else
			{
				TYR_STATIC_ASSERT((std::is_same_v<T, void>), "ConvertFromSRGBtoLinear<T>: Unsupported type.");
				return 0.0f;
			}
		}

		template <typename T>
		static T ConvertFromLinearToSRGB(float value)
		{
			if constexpr (std::is_same_v<T, uint8_t>)
			{
				const float s = LinearToSRGB(value);
				return static_cast<uint8_t>(std::clamp(s, 0.0f, 1.0f) * 255.0f + 0.5f);
			}
			else if constexpr (std::is_same_v<T, float>)
			{
				return LinearToSRGB(value);
			}
			else
			{
				TYR_STATIC_ASSERT((std::is_same_v<T, void>), "ConvertFromLinearToSRGB<T>: Unsupported type.");
				return T{};
			}
		}

		template <typename T>
		static constexpr T GetMaxPixelValue()
		{
			if constexpr (std::is_same_v<T, uint8_t>)
			{
				return 255;
			}
			else if constexpr (std::is_same_v<T, float>)
			{
				return 1.0f;
			}
			else
			{
				TYR_STATIC_ASSERT((std::is_same_v<T, void>), "GetMaxPixelValue<T> not implemented for this type.");
				return T{};
			}
		}
	};
}

namespace std
{
	template <>
	struct hash<tyr::TextureInfo>
	{
		size_t operator()(const tyr::TextureInfo& info) const noexcept
		{
			return std::hash<tyr::uint64>{}(info.GetID());
		}
	};
}
