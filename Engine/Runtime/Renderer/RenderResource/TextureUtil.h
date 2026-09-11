#pragma once

#include "RendererMacros.h"
#include "RenderAPI/RenderAPITypes.h"

namespace tyr
{
	class TYR_RENDERER_API TextureUtil
	{
	public:
		// Returns the pixel size for the specified format in bytes
		static uint CalculateTexelFormatSize(PixelFormat format);

		static float SRGBToLinear(float f);

		static float LinearToSRGB(float f);

		static constexpr uint CalculateMaxMips(uint width, uint height)
		{
			const uint size = std::max(width, height);
			return size ? std::bit_width(size) : 0;
		}

		static constexpr uint CalculateMaxMipsForBlockCompressed(uint width, uint height)
		{
			// Min resolution is 4x4
			const uint size = std::max(width, height);
			return size >= 4 ? std::bit_width(size) - 1 : 0;
		}

		// Currently just supporting BC3 / BC5 and BC7 which have same block dimensions and block size so hard code for now in below 3 functions

		// Returns block width in pixels
		static constexpr uint GetTextureBlockWidth(PixelFormat format)
		{
			switch (format)
			{
			case PF_BC3_UNORM:
			case PF_BC3_SRGB:
			case PF_BC5_UNORM:
			case PF_BC5_SNORM:
			case PF_BC7_UNORM:
			case PF_BC7_SRGB:
				return 4;
			}
			TYR_ASSERT(false);
			return 4;
		}

		// Returns block height in pixels
		static constexpr uint GetTextureBlockHeight(PixelFormat format)
		{
			switch (format)
			{
			case PF_BC3_UNORM:
			case PF_BC3_SRGB:
			case PF_BC5_UNORM:
			case PF_BC5_SNORM:
			case PF_BC7_UNORM:
			case PF_BC7_SRGB:
				return 4;
			}
			TYR_ASSERT(false);
			return 4;
		}

		// Returns size in bytes
		static constexpr uint GetTextureBlockSize(PixelFormat format)
		{
			switch (format)
			{
			case PF_BC3_UNORM:
			case PF_BC3_SRGB:
			case PF_BC5_UNORM:
			case PF_BC5_SNORM:
			case PF_BC7_UNORM:
			case PF_BC7_SRGB:
				return 16;
			}
			TYR_ASSERT(false);
			return 16;
		}

		// Returns total texture size including mips in bytes. Works for BC3, BC5 and BC7
		static constexpr uint64 CalculateBCTextureSize(uint width, uint height, uint mipCount)
		{
			constexpr uint blockW = GetTextureBlockWidth(PixelFormat::PF_BC7_SRGB);
			constexpr uint blockH = GetTextureBlockHeight(PixelFormat::PF_BC7_SRGB);
			constexpr uint bytesPerBlock = GetTextureBlockSize(PixelFormat::PF_BC7_SRGB);

			uint64 totalSize = 0;

			for (uint mip = 0; mip < mipCount; ++mip)
			{
				const uint mipWidth = std::max(1u, width >> mip);
				const uint mipHeight = std::max(1u, height >> mip);

				const uint blocksX = (mipWidth + blockW - 1) / blockW;
				const uint blocksY = (mipHeight + blockH - 1) / blockH;

				totalSize += uint64(blocksX) * blocksY * bytesPerBlock;
			}

			return totalSize;
		}

		static constexpr uint64 CalculateBCTextureSizeWithMaxMips(uint width, uint height)
		{
			const uint mipCount = CalculateMaxMipsForBlockCompressed(width, height);
			return CalculateBCTextureSize(width, height, mipCount);
		}

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
