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

		// Creates a mesh instance - loads the mesh itself plus whatever materials it'll
		// actually need (each override's material, and the mesh's own default for any
		// submesh slot not covered by one) before actually creating it on the renderer.
		// Intended to be called once per MeshComponent (transform + its own overrides,
		// straight from the component - see MaterialOverride's comment). onCreated is invoked
		// with the real MeshInstanceHandle, and the resolved material AssetID used for each
		// submesh slot, once creation actually happens (asynchronous - the mesh/materials
		// might still be loading) - the caller needs both to ever delete the instance later
		// (renderer-side instance plus each material's refcount), since nothing else hands
		// either back.
		void CreateMeshInstance(AssetID meshAssetID, const Matrix4& transform, const LocalArray<MaterialOverride, MeshConstants::c_MaxSubmeshes>& overrides, Function<void(MeshInstanceHandle, const LocalArray<AssetID, MeshConstants::c_MaxSubmeshes>&)> onCreated);

		void LoadLocation(AssetID assetID);

		const char* GetDefaultMaterialPath() const { return c_DefaultMaterialPath; }

		AssetID GetDefaultMaterialAssetID() const { return c_DefaultMaterialAssetID; }

		// Temporary function until mesh loading and mesh components are supported
		Handle GetDefaultMaterial() const { return m_AssetMap.Find(c_DefaultMaterialAssetID)->resourceHandle; }

	private:
		// Called once per frame from the main thread. This is also what calls
		// RendererAPI::Create*/Delete* - those must never be called from anywhere else, since
		// RenderRegistry isn't safe for concurrent creation/deletion against RenderAsync's own
		// reads of it on a worker thread.
		void ProcessPendingAssets();

		// Texture: header load task -> CreateTexture (main thread: GPU texture + upload
		// allocation, kicks off the pixel-load task) -> UploadTexture (main thread: submit
		// the upload request - the pixel-load task already decompressed/read straight into
		// the upload allocation, so there's nothing left to copy here).
		void CreateTexture(TextureHeaderLoadData* ld);
		void UploadTexture(TexturePixelLoadData* ld);
		// Actually frees a texture's renderer resource and AssetData entry - split out of
		// DeleteTexture so ProcessPendingTextureDeletes can call it once a delete that arrived
		// mid-load is finally safe to act on (see DeleteTexture's own comment).
		void DeleteTextureResources(AssetID assetID, AssetData& assetData);
		// Rechecks every AssetID in m_PendingTextureDeletes each frame, actually freeing it once
		// its load has reached Loaded (or dropping it if something re-Loaded it since).
		void ProcessPendingTextureDeletes();

		// Material: file load task -> ResolveMaterialTextures (main thread: kicks off
		// LoadTexture for each dependency, either straight to CreateMaterial if there are
		// none or via m_MaterialsAwaitingTextures otherwise, same as today).
		void ResolveMaterialTextures(MaterialLoadData* ld);
		void CreateMaterial(MaterialLoadData* ld);
		// Same idea as DeleteTextureResources/ProcessPendingTextureDeletes above, for materials.
		void DeleteMaterialResources(AssetID assetID, AssetData& assetData);
		void ProcessPendingMaterialDeletes();

		// Mesh: header load task -> CreateMesh (main thread: creates the render mesh, then
		// GPU vertex/index/meshlet/LOD buffer + upload allocations per LOD, sized straight
		// from the header's chunk data, plus the LOD descriptor upload - which needs no
		// async step, just the allocation offsets already known here - then kicks off one
		// geometry-load task per LOD) -> UploadMeshGeometry (main thread: submits the upload
		// requests - meshlets need no further per-instance work, see MeshChunkMeshlet's comment).
		void CreateMesh(MeshHeaderLoadData* ld);
		void UploadMeshGeometry(MeshGeometryLoadData* ld);
		// Same idea as DeleteTextureResources/ProcessPendingTextureDeletes above, for meshes.
		void DeleteMeshResources(AssetID assetID, AssetData& assetData);
		void ProcessPendingMeshDeletes();

		// Mesh instances: CreateMeshInstance above kicks these off into
		// m_PendingMeshInstances; this resolves them once ready (see the struct's comment
		// in AssetDataTypes.h), called from ProcessPendingAssets each frame.
		void TryResolvePendingMeshInstances();
		AssetID GetEffectiveMaterialForSlot(const MeshHeader& header, const MeshInstanceCreateData& instData, uint slot) const;
		void CreateResolvedMeshInstance(MeshInstanceCreateData* instData);

		HashMap<AssetID, AssetData> m_AssetMap;
		LocalObjectPool<Location, 9, ResetObjectPolicy> m_LocationPool;
		LocalObjectPool<MeshHeader, RenderConstants::c_MaxMeshes, ResetObjectPolicy> m_MeshHeaderPool;
		// Loaded batches ready to be processed
		MPSCRingBuffer<AssetLoadBatch*, 32> m_BatchesLoadedQueue;
		// Headers/files loaded and ready for their main-thread follow-up (GPU/upload
		// allocation requests, dependency resolution) - see the method comments above.
		MPSCRingBuffer<TextureHeaderLoadData*, 32> m_TextureHeadersLoadedQueue;
		MPSCRingBuffer<MaterialLoadData*, 32> m_MaterialFilesLoadedQueue;
		MPSCRingBuffer<MeshHeaderLoadData*, 32> m_MeshHeadersLoadedQueue;
		// Textures that have had their header and raw data loaded but yet to be uploaded to the GPU
		MPSCRingBuffer<TexturePixelLoadData*, 32> m_TexturesLoadedQueue;
		MPSCRingBuffer<MeshGeometryLoadData*, 32> m_MeshesLoadedQueue;
		// Materials that have had the file loaded but waiting on their textures to load
		Array<MaterialLoadData*> m_MaterialsAwaitingTextures;
		// Mesh instances requested but not yet created - waiting on their mesh's header and
		// every material they'll need (see MeshInstanceCreateData's comment).
		Array<MeshInstanceCreateData*> m_PendingMeshInstances;
		// AssetIDs whose Delete* call arrived while still Loading - tearing down a texture/
		// material/mesh's renderer resources or pool slots before its own load has actually
		// finished can race with the in-flight load task/queue entry still writing to that
		// same AssetData or pool slot (see each Delete*'s own comment). refCount is dropped to
		// 0 immediately (so a fresh Load* call naturally resurrects the entry instead of these
		// lists needing to track that themselves) and the actual teardown deferred until
		// ProcessPendingAssets sees loadState reach Loaded. Expected to stay small - a handful
		// of entries at most in ordinary operation, not one of these per asset.
		Array<AssetID> m_PendingTextureDeletes;
		Array<AssetID> m_PendingMaterialDeletes;
		Array<AssetID> m_PendingMeshDeletes;
		Handle m_CurrentBatch;
		Device* m_Device;
		RendererAPI* m_RendererAPI;
		char c_DefaultMaterialPath[PathConstants::c_MaxAssetPathTotalSize];
		AssetID c_DefaultMaterialAssetID;
	};
	
}