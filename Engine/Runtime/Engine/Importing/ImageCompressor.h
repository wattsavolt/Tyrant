#pragma once

#include <Base/Base.h>
#include "EngineMacros.h"
#include "Identifiers/Identifiers.h"

namespace tyr
{
	enum class ImageCompressionInputFormat : uint8
	{
		RGBA_8U = 0,
		RGBA_16F,
		RGBA_32F
	};

	enum class ImageCompressionOutputFormat : uint8
	{
		BC3 = 0,
		BC5,
		BC7
	};

	struct Image2DCompressionDesc
	{
		// Can't be const as swizzling can occur to make the input format compatible for the compression library
		void* image;
		// Path relative to assets folder
		const char* outputFilePath;
		AssetID assetID;
		uint width;
		uint height;
		ImageCompressionInputFormat inputFormat;
		ImageCompressionOutputFormat outputFormat;
		// Must be at least 1 which is the original image
		uint8 mipCount = 1;
		// Is the input and output in sSRGB colour space
		bool isSRGB = false;
	};

	struct CubemapCompressionDesc
	{
		void* data;
		// Absolute path
		const char* inputFilePath;
		// Path relative to assets folder
		const char* outputFilePath;
		AssetID assetID;
		ImageCompressionOutputFormat outputFormat;
		uint8 mip = 0;
		// Is the input and output in sSRGB colour space
		bool isSRGB = false;
	};

	class TYR_ENGINE_EXPORT ImageCompressor final
	{
	public:
		static bool CompressImage2D(const Image2DCompressionDesc& desc);
		static bool CompressCubemap(const CubemapCompressionDesc& desc);
	};
}