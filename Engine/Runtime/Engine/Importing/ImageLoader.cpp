#include "ImageLoader.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace tyr
{
	void ImageLoader::LoadImageInfo(const char* filePath, ImageInfo& fileInfo)
	{
		TYR_ASSERT(stbi_info(filePath, &fileInfo.width, &fileInfo.height, &fileInfo.channelCount) != 0);
		if (stbi_is_16_bit(filePath))
		{
			fileInfo.bitDepth = ImageBitDepth::SixteenBit;
		}
		else if (stbi_is_hdr(filePath))
		{
			fileInfo.bitDepth = ImageBitDepth::ThirtyTwoBit;
		}
		else
		{
			fileInfo.bitDepth = ImageBitDepth::EightBit;
		}
	}

	uint8* ImageLoader::LoadImage8U(const char* filePath, int channelCount)
	{
		int width, height, origChannelCount;
		uint8* image = stbi_load(filePath, &width, &height, &origChannelCount, channelCount);
		TYR_ASSERT(image != nullptr);
		return image;
	}

	uint16* ImageLoader::LoadImage16U(const char* filePath, int channelCount)
	{
		int width, height, origChannelCount;
		uint16* image = stbi_load_16(filePath, &width, &height, &origChannelCount, channelCount);
		TYR_ASSERT(image != nullptr);
		return image;
	}

	float* ImageLoader::LoadImage32F(const char* filePath, int channelCount)
	{
		int width, height, origChannelCount;
		float* image = stbi_loadf(filePath, &width, &height, &origChannelCount, channelCount);
		TYR_ASSERT(image != nullptr);
		return image;
	}

	void ImageLoader::FreeImage(void* image)
	{
		stbi_image_free(image);
	}
}

	