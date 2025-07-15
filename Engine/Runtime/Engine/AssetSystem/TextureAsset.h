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

	struct TextureAsset 
	{
		AssetID id;
		TextureInfo info;
	};
}