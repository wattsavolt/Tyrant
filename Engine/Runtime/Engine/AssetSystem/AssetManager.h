#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "TextureAsset.h"
#include "MaterialAsset.h"
#include "Resources/Texture.h"
#include "RenderDataTypes/Material.h"

namespace tyr
{
	struct AssetData
	{
		uint poolIndex;
		uint refCount;
	};

	struct TextureMetadataLoadData
	{
		TextureMetadata metadata;
		const char* filePath;
	};

	struct MaterialLoadData
	{
		MaterialAssetFile material;
		LocalArray<TextureMetadataLoadData, MaterialConstants::c_MaxTextures> textures;
		const char* filePath;
		TaskID taskID;
	};

	class TYR_ENGINE_EXPORT AssetManager final
	{
	public:
		static constexpr uint c_MaxTextures = 3000;
		static constexpr uint c_MaxMaterials = 1000;
		static constexpr uint c_AssetMapInitialCapacity = c_MaxTextures + c_MaxMaterials;

		AssetManager();
		~AssetManager();

		void Update(float deltaTime);

		// Paths relative to asset directory
		void LoadTexture(const char* filePath);

		void LoadMaterial(const char* filePath);

	private:
		LocalObjectPool<Texture, c_MaxTextures> m_TexturePool;
		LocalObjectPool<Material, c_MaxMaterials> m_MaterialPool;
		LocalObjectPool<MaterialLoadData, 100> m_MaterialLoadDataPool;
		Array<Texture*> m_NewTextures;
		Array<Material*> m_NewMaterials;
		HashMap<AssetID, AssetData> m_AssetMap;
	};
	
}