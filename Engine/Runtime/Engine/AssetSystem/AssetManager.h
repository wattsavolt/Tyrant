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

		// Frees every load-data struct still sitting in a pending queue/list instead of
		// letting it leak. Only safe once nothing can still be pushing into these queues.
		void FreePendingAssets();

		void LoadTexture(AssetID assetID);

		void DeleteTexture(AssetID assetID);

		void LoadMaterial(AssetID assetID);

		void DeleteMaterial(AssetID assetID);

		void LoadMesh(AssetID assetID);

		void DeleteMesh(AssetID assetID);

		// Creates a mesh instance - loads the mesh itself plus whatever materials it'll
		// actually need before creating it on the renderer. onCreated is invoked
		// asynchronously with the real instance handle and each submesh's resolved material.
		void CreateMeshInstance(AssetID meshAssetID, const Matrix4& transform, const LocalArray<MaterialOverride, MeshConstants::c_MaxSubmeshes>& overrides, Function<void(MeshInstanceHandle, const LocalArray<AssetID, MeshConstants::c_MaxSubmeshes>&)> onCreated);

		void LoadLocation(AssetID assetID);

	private:
		// Called once per frame from the main thread - also the only place allowed to
		// create/delete renderer resources, since doing so isn't safe against a worker
		// thread's own concurrent reads.
		void ProcessPendingAssets();

		// Texture loading pipeline: load the header, create the GPU texture and request an
		// upload allocation, then submit the upload once the pixel data is ready.
		void CreateTexture(TextureHeaderLoadData* ld);
		void UploadTexture(TexturePixelLoadData* ld);
		// Frees a texture's renderer resource and AssetData entry, deferred until any
		// in-flight load for it has finished.
		void DeleteTextureResources(AssetID assetID, AssetData& assetData);
		// Rechecks every pending delete each frame, actually freeing it once its load has
		// reached Loaded (or dropping it if something re-loaded it since).
		void ProcessPendingTextureDeletes();

		// Material loading pipeline: load the file, then resolve each texture dependency
		// before creating the material.
		void ResolveMaterialTextures(MaterialLoadData* ld);
		void CreateMaterial(MaterialLoadData* ld);
		// Frees a material's renderer resource and AssetData entry, deferred until any
		// in-flight load for it has finished.
		void DeleteMaterialResources(AssetID assetID, AssetData& assetData);
		void ProcessPendingMaterialDeletes();

		// Mesh loading pipeline: load the header, create GPU geometry/LOD buffers and
		// upload allocations sized from it, then submit the geometry upload once each
		// LOD's data is ready.
		void CreateMesh(MeshHeaderLoadData* ld);
		void UploadMeshGeometry(MeshGeometryLoadData* ld);
		// Frees a mesh's renderer resource and AssetData entry, deferred until any
		// in-flight load for it has finished.
		void DeleteMeshResources(AssetID assetID, AssetData& assetData);
		void ProcessPendingMeshDeletes();

		// Resolves pending mesh instances once their mesh and every needed material are
		// ready, called once per frame.
		void TryResolvePendingMeshInstances();
		AssetID GetEffectiveMaterialForSlot(const MeshHeader& header, const MeshInstanceCreateData& instData, uint slot) const;
		void CreateResolvedMeshInstance(MeshInstanceCreateData* instData);

		HashMap<AssetID, AssetData> m_AssetMap;
		LocalObjectPool<Location, 9, ResetObjectPolicy> m_LocationPool;
		LocalObjectPool<MeshHeader, RenderConstants::c_MaxMeshes, ResetObjectPolicy> m_MeshHeaderPool;
		// Loaded batches ready to be processed
		MPSCRingBuffer<AssetLoadBatch*, 32> m_BatchesLoadedQueue;
		// Headers/files loaded and ready for their main-thread follow-up (GPU/upload
		// allocation requests, dependency resolution).
		MPSCRingBuffer<TextureHeaderLoadData*, 32> m_TextureHeadersLoadedQueue;
		MPSCRingBuffer<MaterialLoadData*, 32> m_MaterialFilesLoadedQueue;
		MPSCRingBuffer<MeshHeaderLoadData*, 32> m_MeshHeadersLoadedQueue;
		// Textures that have had their header and raw data loaded but yet to be uploaded to the GPU
		MPSCRingBuffer<TexturePixelLoadData*, 32> m_TexturesLoadedQueue;
		MPSCRingBuffer<MeshGeometryLoadData*, 32> m_MeshesLoadedQueue;
		// Materials that have had the file loaded but waiting on their textures to load
		Array<MaterialLoadData*> m_MaterialsAwaitingTextures;
		// Mesh instances requested but not yet created - waiting on their mesh's header and
		// every material they'll need.
		Array<MeshInstanceCreateData*> m_PendingMeshInstances;
		// AssetIDs whose delete call arrived while still loading - tearing down renderer
		// resources mid-load would race the in-flight load. refCount drops to 0 immediately
		// (so a fresh load naturally resurrects the entry) and teardown is deferred until loading finishes.
		Array<AssetID> m_PendingTextureDeletes;
		Array<AssetID> m_PendingMaterialDeletes;
		Array<AssetID> m_PendingMeshDeletes;
		Handle m_CurrentBatch;
		Device* m_Device;
		RendererAPI* m_RendererAPI;
	};
	
}