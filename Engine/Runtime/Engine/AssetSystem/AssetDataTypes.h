#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "TextureAsset.h"
#include "MaterialAsset.h"
#include "MeshAsset.h"
#include "AssetID.h"
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

	struct AssetAllocation
	{
		void* data = nullptr;
		size_t size = 0;
	};

	struct AssetData
	{
		Handle metadataHandle;
		Handle resourceHandle;
		uint refCount = 0;
		// CPU load state only
		AssetLoadState loadState = AssetLoadState::Unloaded;
	};

	struct TextureHeaderLoadData 
	{
		AssetID assetID;
		TextureHeader header;
		const char* filePath;
	};

	struct TexturePixelLoadData
	{
		AssetID assetID;
		AssetAllocation allocation;
		TextureHandle texture;
		const char* filePath;
	};

	struct MaterialLoadData
	{
		AssetID assetID;
		LocalArray<AssetID, MaterialConstants::c_MaxTextures> remainingDependencies;
		MaterialAssetFile file;
		const char* filePath;
	};

	struct MeshHeaderLoadData
	{
		AssetID assetID;
		const char* filePath;
		Handle header;
	};

	struct MeshLODGeometryLoadData
	{
		AssetAllocation meshletsAllocation;
		AssetAllocation verticesAllocation;
		AssetAllocation indicesAllocation;
	};

	struct MeshGeometryLoadData
	{
		AssetID assetID;
		MeshHandle mesh;
		const char* filePath;
		LocalArray<MeshLODGeometryLoadData, MeshConstants::c_MaxLods> lods;
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