#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "TextureAsset.h"
#include "MaterialAsset.h"
#include "MeshAsset.h"
#include "SkeletonAsset.h"
#include "AnimationAsset.h"
#include "AssetID.h"
#include "Math/Matrix4.h"
#include "RenderTransfer/RenderTransferTypes.h"

namespace tyr
{
	enum class AssetLoadState : uint8
	{
		Unloaded = 0,
		Loading,
		Loaded
	};

	enum class AssetLoadType : uint8
	{
		TextureMetadata = 0,
		Texture,
		Material,
		Mesh,
		Skeleton,
		Animation
	};

	struct AssetData
	{
		Handle metadataHandle;
		Handle resourceHandle;
		uint refCount = 0;
		// CPU load state only
		AssetLoadState loadState = AssetLoadState::Unloaded;
		// Mesh-specific: how many of this mesh's LODs are still mid-load. Set once (to the
		// LOD count) when its geometry tasks are created, decremented as each one finishes -
		// the mesh only becomes Loaded once every LOD has. Unused by every other asset type.
		uint pendingLodCount = 0;
		// Mesh-specific for the oment: this mesh's header pool slot, set once CreateMesh has deserialized
		// it (default-constructed Handle is invalid until then - check with an explicit
		// bool). Lets other code (AssetManager::LoadMeshInstanceAssets) find a mesh's submesh/
		// default-material list once it's known, without waiting for its geometry to finish
		// uploading too. Unused by every other asset type.
		Handle assetHeader{};
		// GPU bytes the loaded texture or mesh uses. Unused by every other asset type.
		size_t gpuSize = 0;
	};

	// What happens to a texture or mesh once its last reference is deleted.
	enum class AssetDeletePolicy : uint8
	{
		// Kept loaded for a while in case it's loaded again.
		KeepUnreferenced = 0,
		// Freed straight away, e.g. when its file is about to be replaced.
		Immediate
	};

	struct PendingAssetDelete
	{
		AssetID assetID;
		AssetDeletePolicy policy;
	};

	// A loaded texture or mesh with no references left, kept around in case it's loaded again.
	struct UnreferencedAsset
	{
		AssetID assetID;
		float releaseTime;
		AssetLoadType type;
	};

	struct TextureHeaderLoadData
	{
		AssetID assetID;
		TextureHeader header;
		// Owned copy, not a pointer into the registry's storage, which isn't stable once a
		// task reads this later on another thread.
		AssetPath filePath;
	};

	struct TexturePixelLoadData
	{
		AssetID assetID;
		// Where the pixel-load task reads straight into - already GPU-visible staging
		// memory, requested on main thread before the task is created.
		UploadBufferAllocation allocation;
		TextureHandle texture;
		AssetPath filePath;
	};

	struct MaterialLoadData
	{
		AssetID assetID;
		LocalArray<AssetID, MaterialConstants::c_MaxTextures> remainingDependencies;
		MaterialAssetFile file;
		AssetPath filePath;
	};

	struct MeshHeaderLoadData
	{
		AssetID assetID;
		AssetPath filePath;
		Handle header;
		// Exact on-disk size of the serialized MeshHeader, captured from the stream's
		// position right after deserializing it - lets the geometry-load step compute each
		// chunk's absolute file offset (headerByteSize + sum of earlier chunks'
		// compressedBlobSize) without storing offsets in the file or re-parsing the header.
		size_t headerByteSize;
	};

	// A skeleton or animation clip, which only lives on the CPU.
	struct CpuAssetLoadData
	{
		AssetID assetID;
		AssetPath filePath;
		Handle handle;
		AssetLoadType type;
	};

	// One LOD's compressed chunk, in flight from file to GPU buffers. One of these per LOD
	// per mesh - each LOD's geometry loads and uploads independently, since nothing needs
	// to wait for every LOD to be ready before any one of them can be used.
	struct MeshGeometryLoadData
	{
		AssetID assetID;
		MeshHandle mesh;
		// Owned copy, not a pointer into the registry's storage, which isn't stable once a
		// task reads this later on another thread.
		AssetPath filePath;
		uint lodIndex;

		size_t fileOffset;
		uint compressedBlobSize;
		uint decompressedBlobSize;
		uint meshletsOffset;
		uint verticesOffset;
		uint indicesOffset;
		uint decompressedMeshletsSize;
		uint decompressedVerticesSize;
		uint decompressedIndicesSize;
		// Skinned meshes only, otherwise 0.
		uint skinOffset = 0;
		uint decompressedSkinSize = 0;

		// Where the geometry-load task copies decompressed vertices/indices/meshlets to -
		// already GPU-visible staging memory, requested on main thread before the task is
		// created. Meshlets need no further processing before upload - MeshChunkMeshlet is
		// ShaderMeshlet - so the task copies straight into this allocation the same way it does
		// for vertices/indices.
		UploadBufferAllocation verticesUpload;
		UploadBufferAllocation indicesUpload;
		UploadBufferAllocation meshletsUpload;
		UploadBufferAllocation skinUpload;

		GpuBufferAllocation verticesGpuAlloc;
		GpuBufferAllocation indicesGpuAlloc;
		GpuBufferAllocation meshletsGpuAlloc;
		GpuBufferAllocation skinGpuAlloc;
	};

	// The loaded renderer resources a mesh instance draws with.
	struct MeshInstanceAssets
	{
		MeshHandle mesh;
		LocalArray<MaterialHandle, MeshConstants::c_MaxSubmeshes> materials;
		// The material asset each submesh uses: its override, or the mesh's own.
		LocalArray<AssetID, MeshConstants::c_MaxSubmeshes> materialIDs;
	};

	// The assets of a mesh instance, waiting for its mesh header and materials to load.
	struct MeshInstanceAssetsRequest
	{
		AssetID meshAssetID;
		// One material per submesh. An invalid or missing entry uses the mesh's own material.
		LocalArray<AssetID, MeshConstants::c_MaxSubmeshes> materials;
		Function<void(const MeshInstanceAssets&)> onLoaded;
		// Also waits for the mesh's geometry and skeleton.
		bool skinned = false;
		// Set once the materials have been requested, which needs the mesh header to be loaded.
		bool materialsRequested = false;
	};

	struct AssetLoadBatchEntry
	{
		Handle handle;
		AssetLoadType loadType;
	};

	struct AssetLoadBatch
	{
		static constexpr uint c_MaxEntries = 8;
		LocalArray<AssetLoadBatchEntry, c_MaxEntries> loadData;
	};
}