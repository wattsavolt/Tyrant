#include "AssetManager.h"
#include "AssetSystem/MaterialAsset.h"
#include "AssetSystem/AssetUtil.h"
#include "BuildConfig.h"
#include "AssetRegistry.h"

namespace tyr
{
	AssetManager::AssetManager()
		: m_AssetMap(c_MaxAssets)
	{
		AssetRegistry::Instance().Load();
		m_NewTextures.Reserve(200);
		m_NewMaterials.Reserve(100);
	}

	AssetManager::~AssetManager()
	{
		AssetRegistry::Instance().Save();
	}

	void AssetManager::Update(float deltaTime)
	{

	}

	void AssetManager::LoadTexture(const char* filePath)
	{
		AssetID assetID(filePath);
		if (uint* poolIndex = m_AssetMap.Find(assetID))
		{
			AssetData& assetData = m_AssetDataPool.GetObjectRef(*poolIndex);
			assetData.refCount++;
		}
		else
		{
			TextureMetadata textureMetadata;
			AssetUtil::LoadAsset<TextureMetadata>(filePath, textureMetadata);
			TextureDesc desc;
			Texture* texture = m_TexturePool.Create(desc.index);
#if !TYR_FINAL
			char fileName[TYR_MAX_FILENAME_TOTAL_SIZE];
			PathUtil::GetFileNameWithoutExtension(filePath, fileName);
			desc.debugName = fileName;
#endif
			desc.assetID = textureMetadata.assetID;
			desc.info = textureMetadata.info;
			desc.arrayLayerCount = 0;
			// Settings for sampled textures. Different values needed for render targets
			desc.sampleCount = SampleCount::OneBit;
			desc.usage = static_cast<ImageUsage>(IMAGE_USAGE_SAMPLED_BIT | IMAGE_USAGE_TRANSFER_DST_BIT);
			desc.layout = ImageLayout::IMAGE_LAYOUT_GENERAL;
		}
	}

	// TODO: Add async tasks later and use placeholder textures while waiting for raw texture data to load
	void AssetManager::LoadMaterial(const char* filePath)
	{
		AssetID assetID(filePath);
		if (uint* poolIndex = m_AssetMap.Find(assetID))
		{
			AssetData& assetData = m_AssetDataPool.GetObjectRef(*poolIndex);
			assetData.refCount++;
		}
		else
		{
			MaterialAssetFile materialFile;
			AssetUtil::LoadAsset<MaterialAssetFile>(filePath, materialFile);

			uint index;
			Material* material = m_MaterialPool.Create(index);
			material->assetID = materialFile.assetID;
			// Currently the shader array index will be the same index as the pool but this might change later
			material->index = index;
			material->type = materialFile.type;
			AssetRegistry& registry = AssetRegistry::Instance();
			for (AssetID id : materialFile.textures)
			{
				const RegAssetData& assetData = registry.GetAssetData(id);
				LoadTexture(assetData.filePath.CStr());
			}

			uint dataIndex;
			AssetData* assetData = m_AssetDataPool.Create(dataIndex);
			assetData->poolIndex = index;
			assetData->refCount = 1;
			m_AssetMap[assetID] = dataIndex;
			m_NewMaterials.Add(material);
		}
	}
}