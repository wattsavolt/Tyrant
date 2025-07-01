#pragma once

#include "EngineMacros.h"
#include "Core.h"

namespace tyr
{
	struct PbrMaterialImportDesc
	{
		// Input texture paths must be absolute
		const char* albedoPath = nullptr;
		const char* normalPath = nullptr;
		const char* heightPath = nullptr;
		const char* ambientOcclusionPath = nullptr;
		const char* roughnessPath = nullptr;
		const char* metallicPath = nullptr;
		// The path to the folder that will contain the material file. Should be relative to the assets directory.
		const char* outputFolderPath = nullptr;
		const char* materialName = nullptr;
		// If the following AssetIDs are initalized, the existing textures will be used
		AssetID albedoID;
		AssetID normalHeightID;
		AssetID aoRoughnessMetallicID;
		// Are input textures in sRGB colour space
		bool isSRGB = true;
		// For compatibility with PBR materials created for Unity
		bool smoothnessInMetallic = false;
	};

	struct MaterialAssetFile;
	class MaterialImporter final : public INonCopyable
	{
	public:
		static MaterialImporter& Instance();

		// outputFolderPath must be relative to assets director and albedoPath must be absolute
		bool ImportAlbedoTexture(const char* outputFolderPath, const char* textureName, const char* albedoPath, bool isSRGB, AssetID& textureID, AssetID* refID = nullptr) const;
		bool ImportPbrMaterial(const PbrMaterialImportDesc& desc) const;

	private:
		bool SerializeMaterial(const PbrMaterialImportDesc& desc, const MaterialAssetFile& material) const;
		bool CreateMaterialAssetInfo(const char* outputFolderPath, const char* materialName, MaterialAssetFile& material) const;
		bool CreateAlbedo(const PbrMaterialImportDesc& desc, MaterialAssetFile& material) const;
		bool CreateNormalHeight(const PbrMaterialImportDesc& desc, MaterialAssetFile& material) const;
		bool CreateAORoughnessMetallic(const PbrMaterialImportDesc& desc, MaterialAssetFile& material) const;
	};
}