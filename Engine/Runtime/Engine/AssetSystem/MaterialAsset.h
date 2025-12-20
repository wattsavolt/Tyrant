#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "TextureAsset.h"
#include "RenderDataTypes/Material.h"

namespace tyr
{
	static constexpr const char* c_MaterialFileExtension = ".mat";

	struct MaterialAssetFile
	{
		AssetID assetID;
		MaterialType type;
		LocalArray<AssetID, MaterialConstants::c_MaxTextures> textures;
	};
}