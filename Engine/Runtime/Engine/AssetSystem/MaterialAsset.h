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
		uint textureCount;
		AssetID textures[Material::c_MaxTextures];
	};

	struct MaterialAsset 
	{
		AssetID id;
		MaterialType type;
		uint textureCount;
		uint textures[Material::c_MaxTextures];
	};
}