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
		uint width;
		uint height;
		ImageCompressionInputFormat inputFormat;
		ImageCompressionOutputFormat outputFormat;
		// Must be at least 1 which is the original image
		uint8 mipCount = 1;
		// Is the input and output in sSRGB colour space
		bool isSRGB = false;
		// Above zero, every mip keeps the share of texels whose alpha reaches this, so an
		// alpha-tested surface such as a fence doesn't thin out with distance.
		float alphaTestCutoff = 0.0f;
	};

	struct CubemapCompressionDesc
	{
		void* data;
		// Absolute path
		const char* inputFilePath;
		// Path relative to assets folder
		const char* outputFilePath;
		ImageCompressionOutputFormat outputFormat;
		uint8 mip = 0;
		// Is the input and output in sSRGB colour space
		bool isSRGB = false;
	};

	// A cubemap made one face at a time, so only one face's pixels are held at once.
	struct HdrCubemapCompressionDesc
	{
		// Fills one face, faceSize by faceSize RGBA floats, in Vulkan's cubemap face order.
		void (*fillFace)(uint face, float* rgba, uint faceSize, void* userData) = nullptr;
		void* userData = nullptr;
		uint faceSize = 0;
		uint8 mipCount = 1;
		// Path relative to assets folder
		const char* outputFilePath = nullptr;
	};

	class TYR_ENGINE_API ImageCompressor final
	{
	public:
		static bool CompressImage2D(const Image2DCompressionDesc& desc);
		static bool CompressCubemap(const CubemapCompressionDesc& desc);
		// Compresses to BC6H, laid out mip by mip with every face of a mip together, as uploads expect.
		static bool CompressHdrCubemap(const HdrCubemapCompressionDesc& desc);
	};
}