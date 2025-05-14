#pragma once

#include <Base/Base.h>

namespace tyr
{
	struct ImageInfo
	{
		int width;
		int height;
		int channelCount;
	};

	class ImageLoader final
	{
	public:
		static void LoadImageInfo(const char* fileName, ImageInfo& fileInfo);
		static uint8* LoadImage8U(const char* fileName, int channelCount);
		static uint16* LoadImage16U(const char* fileName, int channelCount);
		// Normalizes pixel values to [0.0, 1.0f] range
		static float* LoadImageF(const char* fileName, int channelCount);
		static void FreeImage(void* image);
	};
}