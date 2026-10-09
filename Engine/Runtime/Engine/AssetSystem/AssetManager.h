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

		// The policy only applies when this releases the last reference.
		void DeleteTexture(AssetID assetID, AssetDeletePolicy policy = AssetDeletePolicy::KeepUnreferenced);

		// Returns an invalid handle until the texture has finished loading.
		TextureHandle GetTexture(AssetID assetID) const;

		// Unloaded when the asset isn't loaded or in use.
		AssetLoadState GetLoadState(AssetID assetID) const;

		// Frees the asset now if it's only being kept loaded for reuse, e.g. before its file is deleted.
		void EvictUnreferencedAsset(AssetID assetID);

		void LoadMaterial(AssetID assetID);

		// Materials aren't kept unreferenced - the policy is passed on to their textures.
		void DeleteMaterial(AssetID assetID, AssetDeletePolicy policy = AssetDeletePolicy::KeepUnreferenced);

		void LoadMesh(AssetID assetID);

		void DeleteMesh(AssetID assetID, AssetDeletePolicy policy = AssetDeletePolicy::KeepUnreferenced);

		// Null until LoadMesh has read the mesh's header.
		const MeshHeader* GetMeshHeader(AssetID assetID) const;

		// Invalid until the mesh's header has loaded, which is when its GPU mesh is created.
		MeshHandle GetMesh(AssetID assetID) const;

		// Invalid until the material has finished loading.
		MaterialHandle GetMaterial(AssetID assetID) const;

		// Loads the mesh and its materials, then creates the instance on the renderer. An invalid
		// or missing entry in materials uses the mesh's own material for that submesh. onCreated
		// receives the instance and the material actually used for each submesh.
		void CreateMeshInstance(AssetID meshAssetID, const Matrix4& transform, const LocalArray<AssetID, MeshConstants::c_MaxSubmeshes>& materials, Function<void(MeshInstanceHandle, const LocalArray<AssetID, MeshConstants::c_MaxSubmeshes>&)> onCreated);

		void LoadLocation(AssetID assetID);

	private:
		static constexpr float c_UnreferencedAssetMaxAge = 60.0f;
		// The share of the GPU memory budget beyond which unreferenced textures are let go.
		static constexpr float c_BudgetThreshold = 0.8f;
		// Frames before a deleted texture's memory is given back, once the GPU is done with it.
		static constexpr uint c_EvictionDelayFrames = RenderConstants::c_BufferedFrameCount + 1;
		// Also bounds how many renderer pool slots the cache can hold on to.
		static constexpr uint c_MaxUnreferencedAssets = 256;

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
		void DeleteMaterialResources(AssetID assetID, AssetData& assetData, AssetDeletePolicy policy);
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

		// Defers a delete until loading finishes. Immediate wins if the asset is already pending.
		static void AddPendingDelete(Array<PendingAssetDelete>& pendingDeletes, AssetID assetID, AssetDeletePolicy policy);
		// Called once a loaded texture or mesh has no references left.
		void ReleaseLoadedAsset(AssetID assetID, AssetData& assetData, AssetLoadType type, AssetDeletePolicy policy);

		// Unreferenced texture/mesh cache: a loaded asset whose refCount drops to 0 is kept until
		// it's too old, the cache is over budget, or it's loaded again.
		void AddUnreferencedAsset(AssetID assetID, const AssetData& assetData, AssetLoadType type);
		void ReuseUnreferencedAsset(AssetID assetID, const AssetData& assetData);
		void EvictUnreferencedAssets();
		// Returns the GPU bytes the asset used.
		size_t DeleteUnreferencedAssetResources(const UnreferencedAsset& entry);

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
		Array<PendingAssetDelete> m_PendingTextureDeletes;
		Array<PendingAssetDelete> m_PendingMaterialDeletes;
		Array<PendingAssetDelete> m_PendingMeshDeletes;
		// Oldest release first.
		Array<UnreferencedAsset> m_UnreferencedAssets;
		// Texture bytes evicted in each of the last few frames, whose memory may not be given back yet.
		size_t m_EvictedBytes[c_EvictionDelayFrames] = {};
		uint m_EvictedBytesIndex = 0;
		// Seconds since startup, for unreferenced asset ages.
		float m_Time = 0.0f;
		Handle m_CurrentBatch;
		Device* m_Device;
		RendererAPI* m_RendererAPI;
	};
	
}