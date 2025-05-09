#include "ImageLoader.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace tyr
{
	uchar* ImageLoader::LoadFile(const char* fileName)
	{
		int width, height, origNumChannels, channels;
		TYR_ASSERT(stbi_info(fileName, &width, &height, &channels) != 0);
		uchar* image = stbi_load(fileName, &width, &height, &origNumChannels, channels);
		TYR_ASSERT(image != nullptr);
		return image;
	}
}

	