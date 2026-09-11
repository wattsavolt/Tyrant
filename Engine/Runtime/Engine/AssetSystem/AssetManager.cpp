#include "AssetManager.h"
#include "AssetSystem/MaterialAsset.h"
#include "AssetSystem/AssetUtil.h"
#include "BuildConfig.h"
#include "AssetRegistry.h"
#include "RendererModule.h"
#include "RenderAPI/Device.h"
#include "Rendering/RendererAPI.h"
#include "RenderTransfer/UploadRequest.h"
#include "Memory/TempAllocation.h"
#include "Shaders/ShaderTypes.h"

namespace tyr
{
	AssetManager::AssetManager()
		: m_AssetMap(c_MaxAssets)
		, m_CurrentBatch(0)
	{
		RendererModule* rendererModule;
		TYR_GET_MODULE(RendererModule, rendererModule);
		m_Device = rendererModule->GetDevice();

		m_RendererAPI = rendererModule->GetRendererAPI();

		AssetRegistry::Instance().Load();
		m_MaterialsAwaitingTextures.Reserve(100);

		snprintf(c_DefaultMaterialPath, sizeof(c_DefaultMaterialPath), "%s/%s%s", AssetConstants::c_DefaultMaterialFolderName, AssetConstants::c_DefaultMaterialName, AssetConstants::c_MaterialFileExtension);
		c_DefaultMaterialAssetID = AssetRegistry::Instance().GetAssetID(c_DefaultMaterialPath);
	}

	AssetManager::~AssetManager()
	{
		AssetRegistry::Instance().Save();
	}

	void AssetManager::Update(float deltaTime)
	{
		ProcessPendingAssets();
	}

	void AssetManager::ProcessPendingAssets()
	{
		{
			while (Optional<TexturePixelLoadData*> ld = m_TexturesLoadedQueue.Dequeue())
			{
				UploadTexture(ld.value());
			}
		}

		for (uint i = 0; i < m_MaterialsAwaitingTextures.Size();)
		{
			MaterialLoadData* ld = m_MaterialsAwaitingTextures[i];
			for (uint j = 0; j < ld->remainingDependencies.Size();)
			{
				const AssetData& texAssetData = m_AssetMap[ld->remainingDependencies[j]];
				if (texAssetData.loadState == AssetLoadState::Loaded)
				{
					// Remove the texture but don't swap and pop because order must be maintained
					ld->remainingDependencies.Erase(j);
				}
				else
				{
					++j;
				}
			}
			if (ld->remainingDependencies.IsEmpty())
			{
				CreateMaterial(m_MaterialsAwaitingTextures[i]);
				m_MaterialsAwaitingTextures.SwapAndPopBack(i);
			}
			else
			{
				++i;
			}
		}
	}

	void AssetManager::LoadTexture(AssetID assetID)
	{
		if (AssetData* assetData = m_AssetMap.Find(assetID))
		{
			assetData->refCount++;
		}
		else
		{
			// TODO: Use batches later
			//AssetLoadBatch& batch = TempNew<AssetLoadBatchPool>(m_CurrentBatch);

			AssetData& asset = m_AssetMap[assetID];
			asset.loadState = AssetLoadState::Loading;
			asset.refCount = 1;

			TextureHeaderLoadData* ld = TempNew<TextureHeaderLoadData>();
			ld->assetID = assetID;
			ld->filePath = AssetRegistry::Instance().GetAssetData(assetID).filePath.CStr();

			// Do following in async task	
			AssetUtil::LoadAsset<TextureHeader>(ld->filePath, ld->header);
			// End create async task

			// TODO: Process batch in another function and do following
			CreateTexture(ld);
		}
	}

	void AssetManager::DeleteTexture(AssetID assetID)
	{
		AssetData* assetData = m_AssetMap.Find(assetID);
		TYR_ASSERT(assetData);
		if (assetData->refCount <= 1)
		{
			m_RendererAPI->DeleteTexture(TextureHandle(assetData->resourceHandle));
			m_AssetMap.Erase(assetID);
		}
		else
		{
			assetData->refCount--;
		}
	}

	void AssetManager::CreateTexture(TextureHeaderLoadData* ld)
	{
		TextureDesc desc;
#if !TYR_FINAL
		char fileName[TYR_MAX_FILENAME_TOTAL_SIZE];
		PathUtil::GetFileNameWithoutExtension(ld->filePath, fileName);
		desc.debugName = fileName;
#endif
		desc.info = ld->header.info;
		desc.info.arrayLayerCount = 0;
		// Settings for sampled textures. Different values needed for render targets
		desc.sampleCount = SampleCount::OneBit;
		desc.usage = static_cast<ImageUsage>(IMAGE_USAGE_SAMPLED_BIT | IMAGE_USAGE_TRANSFER_DST_BIT);
		desc.layout = ImageLayout::IMAGE_LAYOUT_GENERAL;

		AssetData& assetData = m_AssetMap[ld->assetID];
		assetData.resourceHandle = m_RendererAPI->CreateTexture(desc).h;

		TexturePixelLoadData* pixelLD = TempNew<TexturePixelLoadData>();
		pixelLD->assetID = ld->assetID;

		pixelLD->allocation.data = TempAlloc(ld->header.dataSize);
		pixelLD->allocation.size = ld->header.dataSize;

		// TODO: Create async task for following 
		const size_t bytesRead = FileStream::ReadLastBytes(ld->filePath, pixelLD->allocation.data, ld->header.dataSize);
		TYR_ASSERT(bytesRead >= ld->header.dataSize);
		// End create async task

		TempDelete<TextureHeaderLoadData>(ld);
		m_TexturesLoadedQueue.Enqueue(pixelLD);
	}

	void AssetManager::UploadTexture(TexturePixelLoadData* ld)
	{
		UploadBufferAllocation uploadAlloc;
		// TODO: Retry if upload is unsuccessful
		const bool allocSuccess = m_RendererAPI->RequestResourceUploadAllocation(ld->allocation.size, uploadAlloc);
		TYR_ASSERT(allocSuccess);

		memcpy(static_cast<uint8*>(uploadAlloc.cpuPtr) + uploadAlloc.offset, ld->allocation.data, ld->allocation.size);

		m_RendererAPI->FlushBufferUploadAllocation(uploadAlloc);

		const TextureInfo& textureInfo = m_RendererAPI->GetTextureInfo(ld->texture);
		TextureUploadRequest request{};
		request.srcBuffer = uploadAlloc.buffer;
		request.srcOffset = uploadAlloc.offset;
		request.dstTexture = ld->texture;
		request.highestMip = 0;
		request.mipCount = textureInfo.mipCount;
		m_RendererAPI->AddTextureUploadRequest(request);

		AssetData& assetData = m_AssetMap[ld->assetID];
		assetData.loadState = AssetLoadState::Loaded;

		TempDelete<TexturePixelLoadData>(ld);
	}

	// TODO: Add async tasks later and use placeholder material while waiting for material to load
	void AssetManager::LoadMaterial(AssetID assetID)
	{
		if (AssetData* assetData = m_AssetMap.Find(assetID))
		{
			assetData->refCount++;
		}
		else
		{
			AssetData& asset = m_AssetMap[assetID];
			asset.loadState = AssetLoadState::Loading;
			asset.refCount = 1;

			AssetRegistry& registry = AssetRegistry::Instance();
			MaterialLoadData* ld = TempNew<MaterialLoadData>();
			ld->assetID = assetID;

			const RegAssetData& regAssetData = registry.GetAssetData(assetID);
			ld->filePath = regAssetData.filePath.CStr();
			
			uint depCount;
			const AssetID* dependencies = registry.GetAssetDependencies(assetID, depCount);
			for (uint i = 0; i < depCount; ++i)
			{
				LoadTexture(dependencies[i]);
			}

			// TODO: Do this in async task in a batch
			AssetUtil::LoadAsset<MaterialAssetFile>(ld->filePath, ld->file);
			for (AssetID id : ld->file.textures)
			{
				const RegAssetData& assetData = registry.GetAssetData(id);
				ld->remainingDependencies.Add(id);
			}
			// End async task


			// TODO: Process batch in another function and do following
			m_MaterialsAwaitingTextures.Add(ld);
		}
	}

	void AssetManager::DeleteMaterial(AssetID assetID)
	{
		AssetData* assetData = m_AssetMap.Find(assetID);
		TYR_ASSERT(assetData);
		if (assetData->refCount <= 1)
		{
			uint depCount;
			const AssetID* dependencies = AssetRegistry::Instance().GetAssetDependencies(assetID, depCount);
			for (uint i = 0; i < depCount; ++i)
			{
				DeleteTexture(dependencies[i]);
			}
			m_RendererAPI->DeleteMaterial(MaterialHandle(assetData->resourceHandle));
			m_AssetMap.Erase(assetID);
		}
		else
		{
			assetData->refCount--;
		}
	}

	void AssetManager::CreateMaterial(MaterialLoadData* ld)
	{
		AssetData& asset = m_AssetMap[ld->assetID];

		MaterialDesc desc;
		desc.type = ld->file.type;
		for (AssetID id : ld->file.textures)
		{
			desc.textures.Add(m_AssetMap[id].resourceHandle);
		}
		
		TempDelete<MaterialLoadData>(ld);

		asset.loadState = AssetLoadState::Loaded;
		asset.resourceHandle = m_RendererAPI->CreateMaterial(desc).h;
	}

	void AssetManager::LoadMesh(AssetID assetID)
	{
		if (AssetData* assetData = m_AssetMap.Find(assetID))
		{
			assetData->refCount++;
		}
		else
		{
			AssetData& asset = m_AssetMap[assetID];
			asset.loadState = AssetLoadState::Loading;
			asset.refCount = 1;

			AssetRegistry& registry = AssetRegistry::Instance();
			MeshHeaderLoadData* ld = TempNew<MeshHeaderLoadData>();
			ld->assetID = assetID;

			const RegAssetData& regAssetData = registry.GetAssetData(assetID);
			ld->filePath = regAssetData.filePath.CStr();

			ld->header = m_MeshHeaderPool.Create();

			MeshHeader& header = m_MeshHeaderPool[ld->header];

			// TODO: Do this in async task in a batch
			AssetUtil::LoadAsset<MeshHeader>(ld->filePath, header);
			// End async task

			// Note: The default material is not loaded in here as it may be overridden by the entity's material component

			// TODO: Process batch in another function and do following
			CreateMesh(ld);
		}
	}

	void AssetManager::DeleteMesh(AssetID assetID)
	{
		AssetData* assetData = m_AssetMap.Find(assetID);
		TYR_ASSERT(assetData);
		if (assetData->refCount <= 1)
		{
			m_RendererAPI->DeleteMesh(MeshHandle(assetData->resourceHandle));
			m_AssetMap.Erase(assetID);
		}
		else
		{
			assetData->refCount--;
		}
	}

	void AssetManager::CreateMesh(MeshHeaderLoadData* ld)
	{
		AssetData& assetData = m_AssetMap[ld->assetID];

		MeshDesc desc;

		assetData.resourceHandle = m_RendererAPI->CreateMesh(desc).h;

		MeshGeometryLoadData* geoLD = TempNew<MeshGeometryLoadData>();
		geoLD->assetID = ld->assetID;

		// TODO: Get the resource buffer upload allocations for transfers to the LOD buffer, meshlet buffer, indices and vertices buffer
		// and get the GPU allocations for these buffers as well. 

		// TODO: Create async task for loading data from file. Don't decompress directly to upload buffer memory as slower to write to than RAM. Use stack allocator
		// for allocations for compressed and decompressed data as will be temporary before writing to the upload buffer.
		
		// End async task

		TempDelete<MeshHeaderLoadData>(ld);
		m_MeshesLoadedQueue.Enqueue(geoLD);
	}

	void AssetManager::LoadLocation(AssetID assetID)
	{
		// TODO: Implement this
		const AssetPath& filePath = AssetRegistry::Instance().GetAssetData(assetID).filePath;
		Handle locationHandle = m_LocationPool.Create();
		/*for (AssetID id : location.textures)
		{

		}*/ 
	}
}