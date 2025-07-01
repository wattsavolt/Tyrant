#pragma once

#include <Base/Base.h>

namespace tyr
{
	enum class ImageBitDepth : uint8
	{
		EightBit = 0,
		SixteenBit,
		ThirtyTwoBit
	};

	struct ImageInfo
	{
		int width;
		int height;
		int channelCount;
		ImageBitDepth bitDepth;
	};

	class ImageLoader final
	{
	public:
		static void LoadImageInfo(const char* filePath, ImageInfo& fileInfo);
		static uint8* LoadImage8U(const char* filePath, int channelCount);
		static uint16* LoadImage16U(const char* filePath, int channelCount);
		// Normalizes pixel values to [0.0, 1.0f] range
		static float* LoadImage32F(const char* filePath, int channelCount);
		static void FreeImage(void* image);
	};
}