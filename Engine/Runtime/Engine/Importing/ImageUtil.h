#pragma once

#include <Base/Base.h>
#include "Resources/Texture.h"

namespace tyr
{
	class ImageUtil final
	{
	public:
		static constexpr uint8 c_MaxChannels = 4;
		static constexpr uint8 c_MaxBytesPerChannel = 4;

        // For interleaved / packed images with an equal size per channel(e.g. RGBA / BGRA)
        template <typename T>
        static void SwapChannels(T * image, uint16 texelCount, uint8 channelCount, uint8 channelIdx0, uint8 channelIdx1)
        {
            TYR_STATIC_ASSERT((std::is_trivial<T>::value), "T must be a trivial type for direct swapping.");

            TYR_ASSERT(image != nullptr);
            TYR_ASSERT(texelCount > 0);
            TYR_ASSERT(channelCount > 0 && channelCount <= c_MaxChannels);
            TYR_ASSERT(channelIdx0 != channelIdx1);
            TYR_ASSERT(channelIdx0 < channelCount);
            TYR_ASSERT(channelIdx1 < channelCount);

            for (uint16 i = 0; i < texelCount; ++i)
            {
                const uint offset = channelCount * i;
                const T temp = image[offset + channelIdx0];
                image[offset + channelIdx0] = image[offset + channelIdx1];
                image[offset + channelIdx1] = temp;
            }
        }

        template <typename T>
        static void InvertChannelLinear(T* image, uint16 texelCount, uint8 channelCount, uint8 channelIdx)
        {
            TYR_STATIC_ASSERT((std::is_trivial<T>::value), "T must be a trivial type for direct swapping.");

            TYR_ASSERT(image != nullptr);
            TYR_ASSERT(texelCount > 0);
            TYR_ASSERT(channelCount > 0 && channelCount <= c_MaxChannels);
            TYR_ASSERT(channelIdx < channelCount);

            static constexpr T maxPixelValue = TextureUtil::GetMaxPixelValue<T>();
            for (uint16 i = 0; i < texelCount; ++i)
            {
                const uint offset = channelCount * i;
                image[offset + channelIdx] = maxPixelValue - image[offset + channelIdx];
            }
        }

        template <typename T>
        static void InvertChannelSRGB(T* image, uint16 texelCount, uint8 channelCount, uint8 channelIdx)
        {
            TYR_STATIC_ASSERT((std::is_trivial<T>::value), "T must be a trivial type for direct swapping.");

            TYR_ASSERT(image != nullptr);
            TYR_ASSERT(texelCount > 0);
            TYR_ASSERT(channelCount > 0 && channelCount <= c_MaxChannels);
            TYR_ASSERT(channelIdx < channelCount);

            static constexpr T maxPixelValue = TextureUtil::GetMaxPixelValue<T>();
            for (uint16 i = 0; i < texelCount; ++i)
            {
                const uint index = channelCount * i + channelIdx;
                const T linearVal = TextureUtil::ConvertFromSRGBtoLinear<uint8>(image[index]);
                image[index] = TextureUtil::ConvertFromLinearToSRGB<uint8>(maxPixelValue - linearVal);
            }
        }

        template <typename T>
        static void InvertChannel(T* image, uint16 texelCount, uint8 channelCount, uint8 channelIdx, bool isSRGB)
        {
            if (isSRGB)
            {
                InvertChannelSRGB(image, texelCount, channelCount, channelIdx);
            }
            else
            {
                InvertChannelLinear(image, texelCount, channelCount, channelIdx);
            }
        }

        template <typename T>
        static void CopyChannel(const T* srcImage, T* dstImage, uint16 texelCount, uint8 srcChannelCount, uint8 dstChannelCount, uint8 srcChannelIdx, uint8 dstChannelIdx)
        {
            TYR_STATIC_ASSERT((std::is_trivial<T>::value), "T must be a trivial type for direct copying.");

            TYR_ASSERT(srcImage != nullptr);
            TYR_ASSERT(dstImage != nullptr);
            TYR_ASSERT(texelCount > 0);
            TYR_ASSERT(srcChannelCount > 0 && srcChannelCount <= c_MaxChannels);
            TYR_ASSERT(dstChannelCount > 0 && dstChannelCount <= c_MaxChannels);
            TYR_ASSERT(srcChannelIdx < srcChannelCount);
            TYR_ASSERT(dstChannelIdx < dstChannelCount);

            for (uint16 i = 0; i < texelCount; ++i)
            {
                dstImage[dstChannelCount * i + dstChannelIdx] = srcImage[srcChannelCount * i + srcChannelIdx];
            }
        }
	};
}