#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "AssetSystem/AssetID.h"
#include "String/Path.h"

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

		// outputFolderPath must be relative to assets director and source path must be absolute
		// Loads a texture as is without modifying or swizzling any channels
		bool ImportTexture(const TextureSource& source, const char* outputFolderPath, const char* textureName, bool isSRGB, AssetID& textureID);
		bool ImportPbrMaterial(const PbrMaterialImportDesc& desc, AssetID& materialID);

		// The default material and its textures. A material imported without one of its
		// textures uses the default one instead. Looked up from the registry on first use.
		const char* GetDefaultMaterialPath() const { return m_DefaultMaterialPath; }
		AssetID GetDefaultMaterialAssetID();
		AssetID GetDefaultAlbedoAssetID();
		AssetID GetDefaultNormalHeightAssetID();
		AssetID GetDefaultAORoughnessMetallicAssetID();

	private:
		MaterialImporter();

		bool CreateAlbedo(const PbrMaterialImportDesc& desc, MaterialAssetFile& material);
		bool CreateNormalHeight(const PbrMaterialImportDesc& desc, MaterialAssetFile& material);
		bool CreateAORoughnessMetallic(const PbrMaterialImportDesc& desc, MaterialAssetFile& material);
		// Returns cachedID, resolving it from the registry first if not already valid.
		AssetID ResolveDefaultTextureAssetID(const char* suffix, AssetID& cachedID);

		char m_DefaultMaterialPath[PathConstants::c_MaxAssetPathTotalSize];
		AssetID m_DefaultMaterialAssetID;
		AssetID m_DefaultAlbedoAssetID;
		AssetID m_DefaultNormalHeightAssetID;
		AssetID m_DefaultAORoughnessMetallicAssetID;
	};
}