#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "TextureAsset.h"
#include "RenderResource/MaterialDesc.h"

namespace tyr
{
	struct MaterialAssetFile
	{
		// Written first. Files from before it start with their texture count, which is never this.
		static constexpr uint c_FileVersion = 0x544D4103;
		// Before the emissive parameters, and before the layer parameters.
		static constexpr uint c_FileVersionNoEmissive = 0x544D4102;
		static constexpr uint c_FileVersionNoLayer = 0x544D4101;

		LocalArray<AssetID, MaterialConstants::c_MaxTextures> textures;
		MaterialType type = MaterialType::PBR;
		MaterialParams params;
	};

	template<> void Serialize(BufferedFileStream& stream, const MaterialAssetFile& file);
	template<> void Deserialize(BufferedFileStream& stream, MaterialAssetFile& file);
}
