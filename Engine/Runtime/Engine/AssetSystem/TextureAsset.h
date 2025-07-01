#pragma once
#include "EngineMacros.h"
#include "Core.h"
#include "Resources/Texture.h"

namespace tyr
{
	static constexpr const char* c_TextureFileExtension = ".tex";

	struct TextureMetadata
	{
		AssetID assetID;
		TextureInfo info;
		uint dataSize;
	};

	struct TextureAsset : public SingleThreadedRefCountedObject
	{
		AssetID id;
		TextureInfo info;
	};
}