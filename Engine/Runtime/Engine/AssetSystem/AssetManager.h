#pragma once

#include "Core.h"
#include "EngineMacros.h"
#include "TextureAsset.h"
#include "MaterialAsset.h"
#include "AssetDataTypes.h"
#include "Level/Level.h"
#include "RenderBase/RenderHandles.h"
#include "AssetConstants.h"
#include "Rendering/RenderConstants.h"

namespace tyr
{
	class Device;
	class RendererAPI;

	class TYR_ENGINE_API AssetManager final
	{
	public:
		static constexpr uint c_MaxAssets = RenderConstants::c_MaxTextures + RenderConstants::c_MaxMaterials;

		AssetManager();
		~AssetManager();

		void Update(float deltaTime);

		void LoadTexture(AssetID assetID);

		void DeleteTexture(AssetID assetID);

		void LoadMaterial(AssetID assetID);

		void DeleteMaterial(AssetID assetID);

		void LoadMesh(AssetID assetID);

		void DeleteMesh(AssetID assetID);

		void LoadLocation(AssetID assetID);

		const char* GetDefaultMaterialPath() const { return c_DefaultMaterialPath; }

		AssetID GetDefaultMaterialAssetID() const { return c_DefaultMaterialAssetID; }

		// Temporary function until mesh loading and mesh components are supported
		Handle GetDefaultMaterial() const { return m_AssetMap.Find(c_DefaultMaterialAssetID)->resourceHandle; }

	private:
		void ProcessPendingAssets();
		void CreateTexture(TextureHeaderLoadData* ld);
		void UploadTexture(TexturePixelLoadData* ld);
		void CreateMaterial(MaterialLoadData* ld);
		void CreateMesh(MeshHeaderLoadData* ld);
		HashMap<AssetID, AssetData> m_AssetMap;
		LocalObjectPool<Location, 9, false> m_LocationPool;
		LocalObjectPool<MeshHeader, RenderConstants::c_MaxMeshes, false> m_MeshHeaderPool;
		// Loaded batches ready to be processed
		MPSCRingBuffer<AssetLoadBatch*, 32> m_BatchesLoadedQueue;
		// Textures that have had their header and raw data loaded but yet to be uploaded to the GPU
		MPSCRingBuffer<TexturePixelLoadData*, 32> m_TexturesLoadedQueue;
		MPSCRingBuffer<MeshGeometryLoadData*, 32> m_MeshesLoadedQueue;
		// Materials that have had the file loaded but waiting on their textures to load
		Array<MaterialLoadData*> m_MaterialsAwaitingTextures;
		Handle m_CurrentBatch;
		Device* m_Device;
		RendererAPI* m_RendererAPI;
		char c_DefaultMaterialPath[PathConstants::c_MaxAssetPathTotalSize];
		AssetID c_DefaultMaterialAssetID;
	};
	
}