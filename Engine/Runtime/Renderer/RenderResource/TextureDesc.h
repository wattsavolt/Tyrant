#pragma once

#include "RenderAPI/RenderAPITypes.h"
#include "RenderAPI/Image.h"
#include "Identifiers/Hashing.h"

namespace tyr
{
	struct TextureInfo
	{
		uint width;
		uint height;
		// depth only used for Image3D image type
		uint depth;
		uint arrayLayerCount;
		uint mipCount;
		PixelFormat format;
		ImageType type;

		bool operator==(const TextureInfo& other) const
		{
			return width == other.width &&
				height == other.height &&
				depth == other.depth &&
				arrayLayerCount == other.arrayLayerCount &&
				mipCount == other.mipCount &&
				format == other.format &&
				type == other.type;
		}
	};

	struct TextureDesc
	{
		TYR_DECLARE_GDEBUGNAME(debugName);
		TextureInfo info;
		SamplerHandle sampler;
		ImageUsage usage;
		SampleCount sampleCount;
		ImageLayout layout;
	};
}

namespace std
{
	template <>
	struct hash<tyr::TextureInfo>
	{
		size_t operator()(const tyr::TextureInfo& info) const
		{
			size_t h = 0;
			tyr::HashCombine(h, std::hash<tyr::uint>()(info.width));
			tyr::HashCombine(h, std::hash<tyr::uint>()(info.height));
			tyr::HashCombine(h, std::hash<tyr::uint>()(info.depth));
			tyr::HashCombine(h, std::hash<tyr::uint>()(info.arrayLayerCount));
			tyr::HashCombine(h, std::hash<tyr::uint>()(info.mipCount));
			tyr::HashCombine(h, std::hash<tyr::uint8>()(static_cast<tyr::uint8>(info.format)));
			tyr::HashCombine(h, std::hash<tyr::uint8>()(static_cast<tyr::uint8>(info.type)));
			return h;
		}
	};
}
