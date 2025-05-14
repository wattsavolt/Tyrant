#include "ImageLoader.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace tyr
{
	void ImageLoader::LoadImageInfo(const char* fileName, ImageInfo& fileInfo)
	{
		TYR_ASSERT(stbi_info(fileName, &fileInfo.width, &fileInfo.height, &fileInfo.channelCount) != 0);
	}

	uint8* ImageLoader::LoadImage8U(const char* fileName, int channelCount)
	{
		int width, height, origChannelCount;
		uint8* image = stbi_load(fileName, &width, &height, &origChannelCount, channelCount);
		TYR_ASSERT(image != nullptr);
		return image;
	}

	uint16* ImageLoader::LoadImage16U(const char* fileName, int channelCount)
	{
		int width, height, origChannelCount;
		uint16* image = stbi_load_16(fileName, &width, &height, &origChannelCount, channelCount);
		TYR_ASSERT(image != nullptr);
		return image;
	}

	float* ImageLoader::LoadImageF(const char* fileName, int channelCount)
	{
		int width, height, origChannelCount;
		float* image = stbi_loadf(fileName, &width, &height, &origChannelCount, channelCount);
		TYR_ASSERT(image != nullptr);
		return image;
	}

	void ImageLoader::FreeImage(void* image)
	{
		stbi_image_free(image);
	}
}

	