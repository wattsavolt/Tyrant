#pragma once

#include "AssetID.h"

namespace tyr
{
	struct AssetConstants
	{
		static constexpr AssetID c_InvalidAssetID = 0;
		static constexpr const char* c_TextureFileExtension = ".tex";
		static constexpr const char* c_MaterialFileExtension = ".mat";
		static constexpr const char* c_MeshFileExtension = ".mesh";
		static constexpr const char* c_SkeletalMeshFileExtension = ".skmesh";
		static constexpr const char* c_LevelFileExtension = ".level";
		static constexpr const char* c_LevelFolderName = "Levels";
		static constexpr const char* c_MaterialFolderName = "Materials";
		static constexpr const char* c_DefaultMaterialName = "DefaultMaterial";
		static constexpr const char* c_DefaultMaterialFolderName = c_DefaultMaterialName;
		// The suffixes MaterialImporter's CreateAlbedo/CreateNormalHeight/CreateAORoughnessMetallic
		// append to a material's own name for each texture they produce - shared here so that
		// code resolving one of those textures by path (e.g. a fallback lookup for a missing
		// texture) can never drift out of sync with what actually gets written on import.
		static constexpr const char* c_AlbedoTextureSuffix = "_Albedo";
		static constexpr const char* c_NormalHeightTextureSuffix = "_NormalHeight";
		static constexpr const char* c_AORoughnessMetallicTextureSuffix = "_AORoughnessMetallic";
	};

}