#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "TextureAsset.h"
#include "RenderResource/MaterialDesc.h"

namespace tyr
{
	struct MaterialAssetFile
	{
		LocalArray<AssetID, MaterialConstants::c_MaxTextures> textures;
		MaterialType type;
	};
}