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
		static constexpr const char* c_MaterialFolderName = "Materials";
		static constexpr const char* c_DefaultMaterialName = "DefaultMaterial";
		static constexpr const char* c_DefaultMaterialFolderName = c_DefaultMaterialName;
	};

}