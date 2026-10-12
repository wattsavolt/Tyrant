#pragma once

#include "Core.h"
#include "EngineMacros.h"
#include "TextureAsset.h"
#include "MaterialAsset.h"
#include "AssetDataTypes.h"
#include "Level/Level.h"
#include "RenderBase/RenderHandles.h"
#include "AssetConstants.h"
#include "CollisionMeshAsset.h"
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

		// Loads a mesh and the materials an instance of it draws with, then passes them to onLoaded
		// so the caller can create the instance. An invalid or missing entry in materials uses the
		// mesh's own material for that submesh. A skinned mesh also waits for its geometry and
		// skeleton. The caller is given a reference to the mesh and each of the materials.
		void LoadMeshInstanceAssets(AssetID meshAssetID, const LocalArray<AssetID, MeshConstants::c_MaxSubmeshes>& materials, bool skinned, Function<void(const MeshInstanceAssets&)> onLoaded);

		// Skeletons and animation clips only live on the CPU, and are freed with their last reference.
		void LoadSkeleton(AssetID assetID);
		void DeleteSkeleton(AssetID assetID);
		// Null until loaded.
		const Skeleton* GetSkeleton(AssetID assetID) const;

		void LoadAnimation(AssetID assetID);
		void DeleteAnimation(AssetID assetID);
		// Null until loaded.
		const AnimationClip* GetAnimation(AssetID assetID) const;

		// A mesh's collision, also CPU only, held while a collider uses it.
		void LoadCollisionMesh(AssetID assetID);
		void DeleteCollisionMesh(AssetID assetID);
		// Null until loaded.
		const CollisionMesh* GetCollisionMesh(AssetID assetID) const;

		void LoadLocation(AssetID assetID);

#if TYR_EDITOR
		// A loaded material's file, for editing. Null until LoadMaterial has finished.
		const MaterialAssetFile* GetMaterialFile(AssetID assetID) const;
		// Changes a loaded material's parameters, drawn from the next frame. SaveMaterial keeps them.
		void SetMaterialParams(AssetID assetID, const MaterialParams& params);
		void SaveMaterial(AssetID assetID);
#endif

	private:
		static constexpr float c_UnreferencedAssetMaxAge = 60.0f;
		// The share of the GPU memory budget beyond which unreferenced textures are let go.
		static constexpr float c_BudgetThreshold = 0.8f;
		// Frames before a deleted texture's memory is given back, once the GPU is done with it.
		static constexpr uint c_EvictionDelayFrames = RenderConstants::c_BufferedFrameCount + 1;
		// Also bounds how many renderer pool slots the cache can hold on to.
		static constexpr uint c_MaxUnreferencedAssets = 256;
		static constexpr uint c_MaxSkeletons = 64;
		static constexpr uint c_MaxAnimations = 1024;
		static constexpr uint c_MaxCollisionMeshes = RenderConstants::c_MaxMeshes;
		// Each finished-load queue holds every load of its kind that can be in flight at once.
		static constexpr uint c_TextureLoadQueueSize = 4096;
		static constexpr uint c_MaterialLoadQueueSize = 1024;
		static constexpr uint c_MeshHeaderLoadQueueSize = 1024;
		static constexpr uint c_MeshLodLoadQueueSize = 8192;
		static constexpr uint c_CpuAssetLoadQueueSize = 4096;
		TYR_STATIC_ASSERT(c_TextureLoadQueueSize >= RenderConstants::c_MaxTextures, "Texture loads could overflow their queue.");
		TYR_STATIC_ASSERT(c_MaterialLoadQueueSize >= RenderConstants::c_MaxMaterials, "Material loads could overflow their queue.");
		TYR_STATIC_ASSERT(c_MeshHeaderLoadQueueSize >= RenderConstants::c_MaxMeshes, "Mesh loads could overflow their queue.");
		TYR_STATIC_ASSERT(c_MeshLodLoadQueueSize >= RenderConstants::c_MaxMeshLODs, "Mesh LOD loads could overflow their queue.");
		TYR_STATIC_ASSERT(c_CpuAssetLoadQueueSize >= c_MaxSkeletons + c_MaxAnimations + c_MaxCollisionMeshes, "CPU asset loads could overflow their queue.");

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

		// Passes on each mesh instance's assets once its mesh and every needed material are
		// ready, called once per frame.
		void TryResolvePendingMeshInstanceAssets();
		AssetID GetEffectiveMaterialForSlot(const MeshHeader& header, const MeshInstanceAssetsRequest& request, uint slot) const;
		void FinishMeshInstanceAssets(MeshInstanceAssetsRequest* request);

		void LoadCpuAsset(AssetID assetID, AssetLoadType type);
		void DeleteCpuAsset(AssetID assetID, AssetLoadType type);
		void FreeCpuAsset(AssetID assetID, const AssetData& assetData, AssetLoadType type);
		void FinishCpuAssetLoad(CpuAssetLoadData* ld);

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
		LocalObjectPool<Skeleton, c_MaxSkeletons, ResetObjectPolicy> m_SkeletonPool;
		LocalObjectPool<AnimationClip, c_MaxAnimations, ResetObjectPolicy> m_AnimationPool;
		LocalObjectPool<CollisionMesh, c_MaxCollisionMeshes, ResetObjectPolicy> m_CollisionMeshPool;
		// Loaded batches ready to be processed
		MPSCRingBuffer<AssetLoadBatch*, 32> m_BatchesLoadedQueue;
		// Headers/files loaded and ready for their main-thread follow-up (GPU/upload
		// allocation requests, dependency resolution).
		MPSCRingBuffer<TextureHeaderLoadData*, c_TextureLoadQueueSize> m_TextureHeadersLoadedQueue;
		MPSCRingBuffer<MaterialLoadData*, c_MaterialLoadQueueSize> m_MaterialFilesLoadedQueue;
		MPSCRingBuffer<MeshHeaderLoadData*, c_MeshHeaderLoadQueueSize> m_MeshHeadersLoadedQueue;
		// Textures that have had their header and raw data loaded but yet to be uploaded to the GPU
		MPSCRingBuffer<TexturePixelLoadData*, c_TextureLoadQueueSize> m_TexturesLoadedQueue;
		MPSCRingBuffer<MeshGeometryLoadData*, c_MeshLodLoadQueueSize> m_MeshesLoadedQueue;
		MPSCRingBuffer<CpuAssetLoadData*, c_CpuAssetLoadQueueSize> m_CpuAssetsLoadedQueue;
		// Materials that have had the file loaded but waiting on their textures to load
		Array<MaterialLoadData*> m_MaterialsAwaitingTextures;
		// Mesh instances' assets still loading - waiting on their mesh's header and every
		// material they'll need.
		Array<MeshInstanceAssetsRequest*> m_PendingMeshInstanceAssets;
		// AssetIDs whose delete call arrived while still loading - tearing down renderer
		// resources mid-load would race the in-flight load. refCount drops to 0 immediately
		// (so a fresh load naturally resurrects the entry) and teardown is deferred until loading finishes.
		Array<PendingAssetDelete> m_PendingTextureDeletes;
		Array<PendingAssetDelete> m_PendingMaterialDeletes;
		Array<PendingAssetDelete> m_PendingMeshDeletes;
		// Oldest release first.
		Array<UnreferencedAsset> m_UnreferencedAssets;
#if TYR_EDITOR
		// Each loaded material's file at its renderer handle's index, so it can be edited and saved.
		Array<MaterialAssetFile> m_MaterialFiles;
#endif
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