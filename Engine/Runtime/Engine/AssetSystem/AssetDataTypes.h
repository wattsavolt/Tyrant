#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "TextureAsset.h"
#include "MaterialAsset.h"
#include "MeshAsset.h"
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
		Mesh
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
		// Mesh-specific: this mesh's header pool slot, set once CreateMesh has deserialized
		// it (default-constructed Handle is invalid until then - check with an explicit
		// bool). Lets other code (AssetManager::CreateMeshInstance) find a mesh's submesh/
		// default-material list once it's known, without waiting for its geometry to finish
		// uploading too. Unused by every other asset type.
		Handle meshHeader;
	};

	// A single per-submesh-slot material override - what a MeshComponent's own override list
	// is made of, and what AssetManager::CreateMeshInstance takes to build an instance's
	// resolved per-slot materials. Slots not covered by any override fall back to the mesh
	// asset's own default (MeshHeader::materials[slot]).
	struct MaterialOverride
	{
		uint submeshSlot;
		AssetID material;
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

		// Where the geometry-load task copies decompressed vertices/indices/meshlets to -
		// already GPU-visible staging memory, requested on main thread before the task is
		// created. Meshlets need no further processing before upload - MeshChunkMeshlet is
		// deliberately byte-identical to ShaderMeshlet (see its own comment) - so the task
		// copies straight into this allocation the same way it does for vertices/indices.
		UploadBufferAllocation verticesUpload;
		UploadBufferAllocation indicesUpload;
		UploadBufferAllocation meshletsUpload;

		GpuBufferAllocation verticesGpuAlloc;
		GpuBufferAllocation indicesGpuAlloc;
		GpuBufferAllocation meshletsGpuAlloc;
	};

	// One AssetManager::CreateMeshInstance call in flight, waiting on its mesh's header (to
	// know the submesh/default-material list) and every material it'll actually need
	// (overrides, plus the mesh's own default for any slot not overridden).
	struct MeshInstanceCreateData
	{
		AssetID meshAssetID;
		Matrix4 transform;
		LocalArray<MaterialOverride, MeshConstants::c_MaxSubmeshes> overrides;
		// Set once this instance's needed materials have actually been requested (via
		// LoadMaterial) - can only happen once the mesh's header is available (need the
		// submesh/default-material list to know what "not overridden" needs loading), so
		// this has to be a one-time, later transition rather than done up front.
		bool materialsRequested = false;
		// Invoked with the real MeshInstanceHandle, and the resolved (override-or-default)
		// material AssetID actually used for each submesh slot, once
		// AssetManager::CreateResolvedMeshInstance actually creates it - the caller has no
		// other way to learn either, since creation is asynchronous. Needed so whoever asked
		// for this instance can delete it later - both the renderer-side instance and this
		// instance's share of each material's refcount (e.g. WorldManager tearing down a
		// world's mesh components - see WorldMeshInstance).
		Function<void(MeshInstanceHandle, const LocalArray<AssetID, MeshConstants::c_MaxSubmeshes>&)> onCreated;
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