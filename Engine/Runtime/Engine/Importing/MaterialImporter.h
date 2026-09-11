#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "AssetSystem/AssetID.h"

namespace tyr
{
	struct TextureSource 
	{
		const char* path = nullptr;
		const uchar* data = nullptr;
		size_t dataSize = 0;

		bool IsPresent() const { return path != nullptr || data != nullptr; }
	};

	// gltf compatible
	struct PbrMaterialImportDesc
	{
		// Input texture paths must be absolute
		// Fill in path or data / size for a texture but not both
		TextureSource albedoSource{};
		TextureSource normalSource{};
		TextureSource heightSource{};
		TextureSource occlusionSource{};
		TextureSource roughnessMetallicSource{};
		// The path to the folder that will contain the material file. Should be relative to the assets directory.
		const char* outputFolderPath = nullptr;
		const char* materialName = nullptr;
		// If the following AssetIDs are initialized, the existing textures will be used
		AssetID albedoID;
		AssetID normalHeightID;
		AssetID aoRoughnessMetallicID;
		// Are input textures in sRGB colour space
		bool isSRGB = true;
	};

	struct MaterialAssetFile;
	struct ImageInfo;
	class TYR_ENGINE_API MaterialImporter final : public INonCopyable
	{
	public:
		static MaterialImporter& Instance();

		// outputFolderPath must be relative to assets director and albedoPath must be absolute
		bool ImportAlbedoTexture(const TextureSource& source, const char* outputFolderPath, const char* textureName, bool isSRGB, AssetID& textureID) const;
		bool ImportPbrMaterial(const PbrMaterialImportDesc& desc) const;

	private:
		bool SerializeMaterial(const PbrMaterialImportDesc& desc, const MaterialAssetFile& material) const;
		bool CreateMaterialAssetInfo(const char* outputFolderPath, const char* materialPath, MaterialAssetFile& material) const;
		bool CreateAlbedo(const PbrMaterialImportDesc& desc, AssetID materialID, MaterialAssetFile& material) const;
		bool CreateNormalHeight(const PbrMaterialImportDesc& desc, AssetID materialID, MaterialAssetFile& material) const;
		bool CreateAORoughnessMetallic(const PbrMaterialImportDesc& desc, AssetID materialID, MaterialAssetFile& material) const;
	};
}