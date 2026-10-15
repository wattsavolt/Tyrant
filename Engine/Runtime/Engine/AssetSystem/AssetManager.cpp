#include "AssetManager.h"
#include "AssetSystem/MaterialAsset.h"
#include "AssetSystem/AssetUtil.h"
#include "BuildConfig.h"
#include "AssetRegistry.h"
#include "RendererModule.h"
#include "RenderAPI/Device.h"
#include "Rendering/RendererAPI.h"
#include "RenderInstance/RenderInstanceDescs.h"
#include "RenderTransfer/UploadRequest.h"
#include "Memory/TempAllocation.h"
#include "Memory/StackAllocation.h"
#include "Shaders/ShaderTypes.h"
#include "Threading/TaskScheduler.h"
#include "IO/BufferedFileStream.h"
#include <zstd.h>

namespace tyr
{
	namespace
	{
		// Reads only the header at the start of a mesh file. Returns the header's size in bytes.
		size_t LoadMeshHeader(const char* filePath, MeshHeader& header)
		{
			char absFilePath[TYR_MAX_PATH_TOTAL_SIZE];
			AssetUtil::CreateFullPath(absFilePath, filePath);

			constexpr size_t c_StreamBufferSize = 65536;
			SmartStack<uint8> streamBuffer = SmartStackAlloc<uint8>((uint)c_StreamBufferSize);
			BufferedFileStream stream(streamBuffer, c_StreamBufferSize, absFilePath, BinaryStream::Operation::Read);

			if (AssetUtil::HasExtension(filePath, AssetConstants::c_SkeletalMeshFileExtension))
			{
				DeserializeSkinnedMesh(stream, header);
			}
			else
			{
				Deserialize<MeshHeader>(stream, header);
			}

			// Left with no LODs, so the mesh loads but is never drawn.
			if (!header.IsReadableVersion())
			{
				TYR_LOG_ERROR("%s was saved in an older mesh format. Reimport it to draw it.", filePath);
				header.Reset();
			}
			return stream.GetOffset();
		}

		template<typename T>
		void LoadAssetFile(const char* filePath, T& data)
		{
			char absFilePath[TYR_MAX_PATH_TOTAL_SIZE];
			AssetUtil::CreateFullPath(absFilePath, filePath);

			constexpr size_t c_StreamBufferSize = 65536;
			SmartStack<uint8> streamBuffer = SmartStackAlloc<uint8>((uint)c_StreamBufferSize);
			BufferedFileStream stream(streamBuffer, c_StreamBufferSize, absFilePath, BinaryStream::Operation::Read);
			Deserialize<T>(stream, data);
		}
	}

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
		m_PendingMeshInstanceAssets.Reserve(100);
		m_PendingTextureDeletes.Reserve(16);
		m_PendingMaterialDeletes.Reserve(16);
		m_PendingMeshDeletes.Reserve(16);
		m_UnreferencedAssets.Reserve(c_MaxUnreferencedAssets);
		m_MaterialTextures.Resize(RenderConstants::c_MaxMaterials);
#if TYR_EDITOR
		m_MaterialFiles.Resize(RenderConstants::c_MaxMaterials);
#endif
	}

	AssetManager::~AssetManager()
	{
		for (const UnreferencedAsset& entry : m_UnreferencedAssets)
		{
			DeleteUnreferencedAssetResources(entry);
		}
		m_UnreferencedAssets.Clear();

		AssetRegistry::Instance().Save();
	}

	void AssetManager::Update(float deltaTime)
	{
		m_Time += deltaTime;
		ProcessPendingAssets();
		EvictUnreferencedAssets();
	}

	void AssetManager::ProcessPendingAssets()
	{
		// Earlier textures first, stopping at the first that still doesn't fit.
		uint created = 0;
		while (created < m_TexturesAwaitingUploadSpace.Size() && CreateTexture(m_TexturesAwaitingUploadSpace[created]))
		{
			++created;
		}
		m_TexturesAwaitingUploadSpace.EraseFromFront(created);

		while (Optional<TextureHeaderLoadData*> ld = m_TextureHeadersLoadedQueue.Dequeue())
		{
			if (!m_TexturesAwaitingUploadSpace.IsEmpty() || !CreateTexture(ld.value()))
			{
				m_TexturesAwaitingUploadSpace.Add(ld.value());
			}
		}

		while (Optional<TexturePixelLoadData*> ld = m_TexturesLoadedQueue.Dequeue())
		{
			UploadTexture(ld.value());
		}

		while (Optional<MaterialLoadData*> ld = m_MaterialFilesLoadedQueue.Dequeue())
		{
			ResolveMaterialTextures(ld.value());
		}

		// Meshes wait for upload space the same way, in load order.
		uint meshesCreated = 0;
		while (meshesCreated < m_MeshesAwaitingUploadSpace.Size() && CreateMesh(m_MeshesAwaitingUploadSpace[meshesCreated]))
		{
			++meshesCreated;
		}
		m_MeshesAwaitingUploadSpace.EraseFromFront(meshesCreated);

		while (Optional<MeshHeaderLoadData*> ld = m_MeshHeadersLoadedQueue.Dequeue())
		{
			if (!m_MeshesAwaitingUploadSpace.IsEmpty() || !CreateMesh(ld.value()))
			{
				m_MeshesAwaitingUploadSpace.Add(ld.value());
			}
		}

		while (Optional<MeshGeometryLoadData*> ld = m_MeshesLoadedQueue.Dequeue())
		{
			UploadMeshGeometry(ld.value());
		}

		while (Optional<CpuAssetLoadData*> ld = m_CpuAssetsLoadedQueue.Dequeue())
		{
			FinishCpuAssetLoad(ld.value());
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

		TryResolvePendingMeshInstanceAssets();

		ProcessPendingTextureDeletes();
		ProcessPendingMaterialDeletes();
		ProcessPendingMeshDeletes();
	}

	void AssetManager::FreePendingAssets()
	{
		// Frees each load-data struct's own allocation without running its normal processing,
		// which would create real renderer/GPU resources for assets about to be torn down
		// anyway. Any resource-upload allocation already reserved is reclaimed separately during shutdown.
		while (Optional<TextureHeaderLoadData*> ld = m_TextureHeadersLoadedQueue.Dequeue())
		{
			TempDelete<TextureHeaderLoadData>(ld.value());
		}
		for (TextureHeaderLoadData* ld : m_TexturesAwaitingUploadSpace)
		{
			TempDelete<TextureHeaderLoadData>(ld);
		}
		m_TexturesAwaitingUploadSpace.Clear();

		while (Optional<TexturePixelLoadData*> ld = m_TexturesLoadedQueue.Dequeue())
		{
			TempDelete<TexturePixelLoadData>(ld.value());
		}

		while (Optional<MaterialLoadData*> ld = m_MaterialFilesLoadedQueue.Dequeue())
		{
			TempDelete<MaterialLoadData>(ld.value());
		}

		while (Optional<MeshHeaderLoadData*> ld = m_MeshHeadersLoadedQueue.Dequeue())
		{
			TempDelete<MeshHeaderLoadData>(ld.value());
		}
		for (MeshHeaderLoadData* ld : m_MeshesAwaitingUploadSpace)
		{
			TempDelete<MeshHeaderLoadData>(ld);
		}
		m_MeshesAwaitingUploadSpace.Clear();

		while (Optional<MeshGeometryLoadData*> ld = m_MeshesLoadedQueue.Dequeue())
		{
			TempDelete<MeshGeometryLoadData>(ld.value());
		}

		while (Optional<CpuAssetLoadData*> ld = m_CpuAssetsLoadedQueue.Dequeue())
		{
			TempDelete<CpuAssetLoadData>(ld.value());
		}

		for (MaterialLoadData* ld : m_MaterialsAwaitingTextures)
		{
			TempDelete<MaterialLoadData>(ld);
		}
		m_MaterialsAwaitingTextures.Clear();

		for (MeshInstanceAssetsRequest* request : m_PendingMeshInstanceAssets)
		{
			TempDelete<MeshInstanceAssetsRequest>(request);
		}
		m_PendingMeshInstanceAssets.Clear();
	}

	void AssetManager::LoadTexture(AssetID assetID)
	{
		if (AssetData* assetData = m_AssetMap.Find(assetID))
		{
			ReuseUnreferencedAsset(assetID, *assetData);
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
			ld->filePath = AssetRegistry::Instance().GetAssetData(assetID).filePath;

			// TempNew above (and every other allocation in this pipeline) must stay on the
			// main thread - TempAllocator is thread-local, so a worker thread would
			// allocate/free through a different instance than this one.
			TaskScheduler::Instance().CreateAndEnqueueTask([this, ld]()
			{
				AssetUtil::LoadAsset<TextureHeader>(ld->filePath.CStr(), ld->header);
				m_TextureHeadersLoadedQueue.Enqueue(ld);
			});
		}
	}

	void AssetManager::DeleteTexture(AssetID assetID, AssetDeletePolicy policy)
	{
		AssetData* assetData = m_AssetMap.Find(assetID);
		TYR_ASSERT(assetData);
		if (assetData->refCount <= 1)
		{
			assetData->refCount = 0;
			if (assetData->loadState == AssetLoadState::Loaded)
			{
				ReleaseLoadedAsset(assetID, *assetData, AssetLoadType::Texture, policy);
			}
			else
			{
				// Still loading (pixel data not read in yet) - the main-thread follow-up reads
				// this same AssetData and the renderer's pool slot, so freeing either now
				// could race with it.
				AddPendingDelete(m_PendingTextureDeletes, assetID, policy);
			}
		}
		else
		{
			assetData->refCount--;
		}
	}

	TextureHandle AssetManager::GetTexture(AssetID assetID) const
	{
		const AssetData* assetData = m_AssetMap.Find(assetID);
		if (!assetData || assetData->loadState != AssetLoadState::Loaded)
		{
			return {};
		}
		return TextureHandle(assetData->resourceHandle);
	}

	AssetLoadState AssetManager::GetLoadState(AssetID assetID) const
	{
		const AssetData* assetData = m_AssetMap.Find(assetID);
		return assetData ? assetData->loadState : AssetLoadState::Unloaded;
	}

	void AssetManager::EvictUnreferencedAsset(AssetID assetID)
	{
		for (uint i = 0; i < m_UnreferencedAssets.Size(); ++i)
		{
			if (m_UnreferencedAssets[i].assetID == assetID)
			{
				DeleteUnreferencedAssetResources(m_UnreferencedAssets[i]);
				m_UnreferencedAssets.Erase(i);
				return;
			}
		}
	}

	void AssetManager::EvictAllUnreferencedAssets()
	{
		for (const UnreferencedAsset& entry : m_UnreferencedAssets)
		{
			DeleteUnreferencedAssetResources(entry);
		}
		m_UnreferencedAssets.Clear();
	}

	bool AssetManager::HasPendingDeletes() const
	{
		return !m_PendingTextureDeletes.IsEmpty() || !m_PendingMaterialDeletes.IsEmpty() || !m_PendingMeshDeletes.IsEmpty();
	}

	void AssetManager::AddPendingDelete(Array<PendingAssetDelete>& pendingDeletes, AssetID assetID, AssetDeletePolicy policy)
	{
		// Already pending when it was deleted, loaded and deleted again before finishing loading.
		for (PendingAssetDelete& pending : pendingDeletes)
		{
			if (pending.assetID == assetID)
			{
				if (policy == AssetDeletePolicy::Immediate)
				{
					pending.policy = policy;
				}
				return;
			}
		}
		pendingDeletes.Add({ assetID, policy });
	}

	void AssetManager::ReleaseLoadedAsset(AssetID assetID, AssetData& assetData, AssetLoadType type, AssetDeletePolicy policy)
	{
		if (policy == AssetDeletePolicy::KeepUnreferenced)
		{
			AddUnreferencedAsset(assetID, assetData, type);
		}
		else if (type == AssetLoadType::Texture)
		{
			DeleteTextureResources(assetID, assetData);
		}
		else
		{
			DeleteMeshResources(assetID, assetData);
		}
	}

	void AssetManager::AddUnreferencedAsset(AssetID assetID, const AssetData& assetData, AssetLoadType type)
	{
		// Eviction waits for Update, since callers may still hold AssetData references.
		m_UnreferencedAssets.Add({ assetID, m_Time, type });
	}

	void AssetManager::ReuseUnreferencedAsset(AssetID assetID, const AssetData& assetData)
	{
		// Only a loaded asset with no references can be in the cache.
		if (assetData.refCount != 0 || assetData.loadState != AssetLoadState::Loaded)
		{
			return;
		}

		// Not found when its pending delete hasn't been processed yet this frame.
		for (uint i = 0; i < m_UnreferencedAssets.Size(); ++i)
		{
			if (m_UnreferencedAssets[i].assetID == assetID)
			{
				// Erase rather than swap to keep the oldest-first order.
				m_UnreferencedAssets.Erase(i);
				return;
			}
		}
	}

	void AssetManager::EvictUnreferencedAssets()
	{
		// A texture deleted in the last few frames only gives its memory back once the GPU has
		// finished with it, so what those freed is counted as gone already.
		m_EvictedBytesIndex = (m_EvictedBytesIndex + 1) % c_EvictionDelayFrames;
		m_EvictedBytes[m_EvictedBytesIndex] = 0;
		size_t pendingFree = 0;
		for (size_t evicted : m_EvictedBytes)
		{
			pendingFree += evicted;
		}

		size_t usage;
		size_t budget;
		m_Device->GetDeviceMemoryBudget(usage, budget);
		size_t remainingUsage = usage > pendingFree ? usage - pendingFree : 0;
		const size_t threshold = static_cast<size_t>(static_cast<double>(budget) * c_BudgetThreshold);

		// Kept entries are packed down in place, so the oldest-first order holds.
		const uint count = m_UnreferencedAssets.Size();
		uint keptCount = 0;
		for (uint i = 0; i < count; ++i)
		{
			const UnreferencedAsset entry = m_UnreferencedAssets[i];
			const bool tooOld = m_Time - entry.releaseTime >= c_UnreferencedAssetMaxAge;
			const bool overCount = keptCount + (count - i) > c_MaxUnreferencedAssets;
			// Only textures count against the budget, since meshes live in shared buffers.
			const bool overBudget = entry.type == AssetLoadType::Texture && remainingUsage > threshold;
			if (!tooOld && !overCount && !overBudget)
			{
				m_UnreferencedAssets[keptCount++] = entry;
				continue;
			}

			const size_t freed = DeleteUnreferencedAssetResources(entry);
			if (entry.type == AssetLoadType::Texture)
			{
				m_EvictedBytes[m_EvictedBytesIndex] += freed;
				remainingUsage = remainingUsage > freed ? remainingUsage - freed : 0;
			}
		}
		m_UnreferencedAssets.EraseFromEnd(count - keptCount);
	}

	size_t AssetManager::DeleteUnreferencedAssetResources(const UnreferencedAsset& entry)
	{
		AssetData& assetData = m_AssetMap[entry.assetID];
		TYR_ASSERT(assetData.refCount == 0 && assetData.loadState == AssetLoadState::Loaded);
		const size_t gpuSize = assetData.gpuSize;
		if (entry.type == AssetLoadType::Texture)
		{
			DeleteTextureResources(entry.assetID, assetData);
		}
		else
		{
			DeleteMeshResources(entry.assetID, assetData);
		}
		return gpuSize;
	}

	const MeshHeader* AssetManager::GetMeshHeader(AssetID assetID) const
	{
		const AssetData* assetData = m_AssetMap.Find(assetID);
		if (!assetData || !static_cast<bool>(assetData->assetHeader))
		{
			return nullptr;
		}
		return &m_MeshHeaderPool[assetData->assetHeader];
	}

	MeshHandle AssetManager::GetMesh(AssetID assetID) const
	{
		const AssetData* assetData = m_AssetMap.Find(assetID);
		return assetData && static_cast<bool>(assetData->assetHeader) ? MeshHandle(assetData->resourceHandle) : MeshHandle{};
	}

	MaterialHandle AssetManager::GetMaterial(AssetID assetID) const
	{
		const AssetData* assetData = m_AssetMap.Find(assetID);
		return assetData && assetData->loadState == AssetLoadState::Loaded ? MaterialHandle(assetData->resourceHandle) : MaterialHandle{};
	}

	void AssetManager::DeleteTextureResources(AssetID assetID, AssetData& assetData)
	{
		m_RendererAPI->DeleteTexture(TextureHandle(assetData.resourceHandle));
		m_AssetMap.Erase(assetID);
	}

	void AssetManager::ProcessPendingTextureDeletes()
	{
		for (uint i = 0; i < m_PendingTextureDeletes.Size();)
		{
			const PendingAssetDelete pending = m_PendingTextureDeletes[i];
			AssetData* assetData = m_AssetMap.Find(pending.assetID);
			// A missing entry, or a refCount that's no longer 0, means something else (a fresh
			// Load* call) already resolved this one way or another - nothing left to do here.
			if (!assetData || assetData->refCount != 0)
			{
				m_PendingTextureDeletes.SwapAndPopBack(i);
				continue;
			}

			if (assetData->loadState == AssetLoadState::Loaded)
			{
				ReleaseLoadedAsset(pending.assetID, *assetData, AssetLoadType::Texture, pending.policy);
				m_PendingTextureDeletes.SwapAndPopBack(i);
			}
			else
			{
				++i;
			}
		}
	}

	bool AssetManager::CreateTexture(TextureHeaderLoadData* ld)
	{
		// The upload space is reserved first, so nothing is made for a texture that has to wait.
		UploadBufferAllocation allocation;
		if (!m_RendererAPI->RequestResourceUploadAllocation(ld->header.dataSize, allocation))
		{
			return false;
		}

		TextureDesc desc;
#if !TYR_FINAL
		char fileName[TYR_MAX_FILENAME_TOTAL_SIZE];
		PathUtil::GetFileNameWithoutExtension(ld->filePath.CStr(), fileName);
		desc.debugName = fileName;
#endif
		desc.info = ld->header.info;
		// Settings for sampled textures. Different values needed for render targets
		desc.sampleCount = SampleCount::OneBit;
		desc.usage = static_cast<ImageUsage>(IMAGE_USAGE_SAMPLED_BIT | IMAGE_USAGE_TRANSFER_DST_BIT);
		desc.layout = ImageLayout::IMAGE_LAYOUT_GENERAL;

		AssetData& assetData = m_AssetMap[ld->assetID];
		const TextureHandle textureHandle = m_RendererAPI->CreateTexture(desc);
		assetData.resourceHandle = textureHandle.h;
		assetData.gpuSize = ld->header.dataSize;

		TexturePixelLoadData* pixelLD = TempNew<TexturePixelLoadData>();
		pixelLD->assetID = ld->assetID;
		pixelLD->texture = textureHandle;
		pixelLD->filePath = ld->filePath;

		pixelLD->allocation = allocation;

		TempDelete<TextureHeaderLoadData>(ld);

		// Reads straight into the already-reserved upload allocation - no separate CPU-side
		// buffer or memcpy needed, since a texture's raw pixel bytes need no processing
		// before upload.
		TaskScheduler::Instance().CreateAndEnqueueTask([this, pixelLD]()
		{
			char absFilePath[TYR_MAX_PATH_TOTAL_SIZE];
			AssetUtil::CreateFullPath(absFilePath, pixelLD->filePath.CStr());
			const size_t bytesRead = FileStream::ReadLastBytes(absFilePath, pixelLD->allocation.cpuPtr, pixelLD->allocation.size);
			TYR_ASSERT(bytesRead >= pixelLD->allocation.size);
			m_TexturesLoadedQueue.Enqueue(pixelLD);
		});
		return true;
	}

	void AssetManager::UploadTexture(TexturePixelLoadData* ld)
	{
		m_RendererAPI->FlushBufferUploadAllocation(ld->allocation);

		const TextureInfo& textureInfo = m_RendererAPI->GetTextureInfo(ld->texture);
		TextureUploadRequest request{};
		request.srcBuffer = ld->allocation.buffer;
		request.srcOffset = ld->allocation.offset;
		request.dstTexture = ld->texture;
		request.highestMip = 0;
		request.mipCount = textureInfo.mipCount;
		request.resourceId = ld->allocation.resourceId;
		m_RendererAPI->AddTextureUploadRequest(request);

		AssetData& assetData = m_AssetMap[ld->assetID];
		assetData.loadState = AssetLoadState::Loaded;

		TempDelete<TexturePixelLoadData>(ld);
	}

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
			ld->filePath = regAssetData.filePath;

			// The registry already knows a material's texture dependencies from import time,
			// so these can start loading in parallel with the material file itself below,
			// rather than waiting on the file load to find out what to depend on.
			uint depCount;
			const AssetID* dependencies = registry.GetAssetDependencies(assetID, depCount);
			for (uint i = 0; i < depCount; ++i)
			{
				LoadTexture(dependencies[i]);
			}

			TaskScheduler::Instance().CreateAndEnqueueTask([this, ld]()
			{
				AssetUtil::LoadAsset<MaterialAssetFile>(ld->filePath.CStr(), ld->file);
				m_MaterialFilesLoadedQueue.Enqueue(ld);
			});
		}
	}

	void AssetManager::DeleteMaterial(AssetID assetID, AssetDeletePolicy policy)
	{
		AssetData* assetData = m_AssetMap.Find(assetID);
		TYR_ASSERT(assetData);
		if (assetData->refCount <= 1)
		{
			assetData->refCount = 0;
			if (assetData->loadState == AssetLoadState::Loaded)
			{
				DeleteMaterialResources(assetID, *assetData, policy);
			}
			else
			{
				// Still loading (file parse/texture-dependency resolution in flight) - the
				// renderer resource doesn't exist yet, and tearing down now could race with
				// the main-thread follow-up still reading each texture dependency's AssetData.
				AddPendingDelete(m_PendingMaterialDeletes, assetID, policy);
			}
		}
		else
		{
			assetData->refCount--;
		}
	}

	void AssetManager::DeleteMaterialResources(AssetID assetID, AssetData& assetData, AssetDeletePolicy policy)
	{
		// Read first, since freeing a texture below erases from the map that holds assetData.
		const MaterialHandle material(assetData.resourceHandle);
		const LocalArray<AssetID, MaterialConstants::c_MaxTextures> textures = m_MaterialTextures[material.h.index];
		m_MaterialTextures[material.h.index].Clear();
		for (AssetID texture : textures)
		{
			DeleteTexture(texture, policy);
		}
		m_RendererAPI->DeleteMaterial(material);
		m_AssetMap.Erase(assetID);
	}

	void AssetManager::ProcessPendingMaterialDeletes()
	{
		for (uint i = 0; i < m_PendingMaterialDeletes.Size();)
		{
			const PendingAssetDelete pending = m_PendingMaterialDeletes[i];
			AssetData* assetData = m_AssetMap.Find(pending.assetID);
			if (!assetData || assetData->refCount != 0)
			{
				m_PendingMaterialDeletes.SwapAndPopBack(i);
				continue;
			}

			if (assetData->loadState == AssetLoadState::Loaded)
			{
				DeleteMaterialResources(pending.assetID, *assetData, pending.policy);
				m_PendingMaterialDeletes.SwapAndPopBack(i);
			}
			else
			{
				++i;
			}
		}
	}

	void AssetManager::ResolveMaterialTextures(MaterialLoadData* ld)
	{
		for (AssetID id : ld->file.textures)
		{
			ld->remainingDependencies.Add(id);
		}

		if (ld->remainingDependencies.IsEmpty())
		{
			CreateMaterial(ld);
		}
		else
		{
			m_MaterialsAwaitingTextures.Add(ld);
		}
	}

	void AssetManager::CreateMaterial(MaterialLoadData* ld)
	{
		AssetData& asset = m_AssetMap[ld->assetID];

		MaterialDesc desc;
		desc.type = ld->file.type;
		desc.params = ld->file.params;
		for (AssetID id : ld->file.textures)
		{
			desc.textures.Add(m_AssetMap[id].resourceHandle);
		}

		asset.loadState = AssetLoadState::Loaded;
		asset.resourceHandle = m_RendererAPI->CreateMaterial(desc).h;
		m_MaterialTextures[asset.resourceHandle.index] = ld->file.textures;
#if TYR_EDITOR
		m_MaterialFiles[asset.resourceHandle.index] = ld->file;
#endif
		TempDelete<MaterialLoadData>(ld);
	}

#if TYR_EDITOR
	const MaterialAssetFile* AssetManager::GetMaterialFile(AssetID assetID) const
	{
		const MaterialHandle material = GetMaterial(assetID);
		return material ? &m_MaterialFiles[material.h.index] : nullptr;
	}

	void AssetManager::SetMaterialParams(AssetID assetID, const MaterialParams& params)
	{
		const MaterialHandle material = GetMaterial(assetID);
		TYR_ASSERT(material);
		MaterialAssetFile& file = m_MaterialFiles[material.h.index];
		file.params = params;

		MaterialDesc desc;
		desc.type = file.type;
		desc.params = params;
		for (AssetID id : file.textures)
		{
			desc.textures.Add(m_AssetMap[id].resourceHandle);
		}
		m_RendererAPI->UpdateMaterial(material, desc);
	}

	bool AssetManager::SetMaterialLayer(AssetID assetID, AssetID layerMaterial)
	{
		const MaterialHandle material = GetMaterial(assetID);
		TYR_ASSERT(material);
		MaterialAssetFile& file = m_MaterialFiles[material.h.index];
		LocalArray<AssetID, MaterialConstants::c_PbrLayerTextureCount> layerTextures;
		if (AssetUtil::IsValidAssetID(layerMaterial))
		{
			const MaterialAssetFile* layerFile = GetMaterialFile(layerMaterial);
			if (!layerFile || layerFile->textures.Size() < MaterialConstants::c_PbrLayerTextureCount)
			{
				return false;
			}
			for (uint i = 0; i < MaterialConstants::c_PbrLayerTextureCount; ++i)
			{
				layerTextures.Add(layerFile->textures[i]);
			}
		}

		// The new textures are already loaded by the layer material, so these only add references.
		for (AssetID texture : layerTextures)
		{
			LoadTexture(texture);
		}
		for (uint i = MaterialConstants::c_PbrLayerAlbedoIndex; i < file.textures.Size(); ++i)
		{
			DeleteTexture(file.textures[i], AssetDeletePolicy::KeepUnreferenced);
		}
		file.textures.Resize(std::min<uint>(file.textures.Size(), MaterialConstants::c_PbrLayerAlbedoIndex));
		for (AssetID texture : layerTextures)
		{
			file.textures.Add(texture);
		}
		m_MaterialTextures[material.h.index] = file.textures;

		// Deleting the material later releases whatever the registry lists it as depending on.
		AssetRegistry& registry = AssetRegistry::Instance();
		registry.AddOrUpdateAsset(registry.GetAssetData(assetID).filePath.CStr(), file.textures.Data(), file.textures.Size());
		registry.Save();
		SetMaterialParams(assetID, file.params);
		SaveMaterial(assetID);
		return true;
	}

	void AssetManager::SaveMaterial(AssetID assetID)
	{
		const MaterialHandle material = GetMaterial(assetID);
		TYR_ASSERT(material);
		AssetUtil::SaveAsset<MaterialAssetFile>(AssetRegistry::Instance().GetAssetData(assetID).filePath.CStr(), m_MaterialFiles[material.h.index]);
	}
#endif

	void AssetManager::LoadMesh(AssetID assetID)
	{
		if (AssetData* assetData = m_AssetMap.Find(assetID))
		{
			ReuseUnreferencedAsset(assetID, *assetData);
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
			ld->filePath = regAssetData.filePath;

			// Pool create/destroy has to stay on the thread that owns the pool (main) - only
			// reading/writing the header's already-allocated slot is safe from the task below.
			ld->header = m_MeshHeaderPool.Create();

			TaskScheduler::Instance().CreateAndEnqueueTask([this, ld]()
			{
				// The header's size on disk is where the mesh's chunks start.
				ld->headerByteSize = LoadMeshHeader(ld->filePath.CStr(), m_MeshHeaderPool[ld->header]);
				m_MeshHeadersLoadedQueue.Enqueue(ld);
			});
		}
	}

	void AssetManager::DeleteMesh(AssetID assetID, AssetDeletePolicy policy)
	{
		AssetData* assetData = m_AssetMap.Find(assetID);
		TYR_ASSERT(assetData);
		if (assetData->refCount <= 1)
		{
			assetData->refCount = 0;
			if (assetData->loadState == AssetLoadState::Loaded)
			{
				ReleaseLoadedAsset(assetID, *assetData, AssetLoadType::Mesh, policy);
			}
			else
			{
				// Still loading (header and/or LOD geometry not finished yet) - the main-thread
				// follow-up still reads/writes this same AssetData, and freeing the header pool
				// slot now would leave it working with a deleted (or since-reused) slot.
				AddPendingDelete(m_PendingMeshDeletes, assetID, policy);
			}
		}
		else
		{
			assetData->refCount--;
		}
	}

	void AssetManager::DeleteMeshResources(AssetID assetID, AssetData& assetData)
	{
		m_RendererAPI->DeleteMesh(MeshHandle(assetData.resourceHandle));
		const MeshHeader& header = m_MeshHeaderPool[assetData.assetHeader];
		if (header.IsSkinned())
		{
			DeleteSkeleton(header.skeleton);
		}
		m_MeshHeaderPool.Delete(assetData.assetHeader);
		m_AssetMap.Erase(assetID);
	}

	void AssetManager::ProcessPendingMeshDeletes()
	{
		for (uint i = 0; i < m_PendingMeshDeletes.Size();)
		{
			const PendingAssetDelete pending = m_PendingMeshDeletes[i];
			AssetData* assetData = m_AssetMap.Find(pending.assetID);
			if (!assetData || assetData->refCount != 0)
			{
				m_PendingMeshDeletes.SwapAndPopBack(i);
				continue;
			}

			if (assetData->loadState == AssetLoadState::Loaded)
			{
				ReleaseLoadedAsset(pending.assetID, *assetData, AssetLoadType::Mesh, pending.policy);
				m_PendingMeshDeletes.SwapAndPopBack(i);
			}
			else
			{
				++i;
			}
		}
	}

	bool AssetManager::CreateMesh(MeshHeaderLoadData* ld)
	{
		MeshHeader& header = m_MeshHeaderPool[ld->header];

		// Every upload allocation below must fit, so nothing is made for a mesh that has to wait.
		LocalArray<size_t, MeshConstants::c_MaxLods * 5> uploadSizes;
		for (uint chunkIndex = 0; chunkIndex < header.chunks.Size(); ++chunkIndex)
		{
			const MeshChunkHeader& chunkHeader = header.chunks[chunkIndex];
			uploadSizes.Add(chunkHeader.decompressedVerticesSize);
			uploadSizes.Add(chunkHeader.decompressedIndicesSize);
			uploadSizes.Add(chunkHeader.decompressedMeshletsSize);
			if (header.IsSkinned() && header.chunkSkins[chunkIndex].decompressedSkinSize > 0)
			{
				uploadSizes.Add(header.chunkSkins[chunkIndex].decompressedSkinSize);
			}
			uploadSizes.Add(sizeof(ShaderMeshLOD));
		}
		if (!m_RendererAPI->CanRequestResourceUploadAllocations(uploadSizes.Data(), uploadSizes.Size()))
		{
			return false;
		}

		MeshDesc desc;
		desc.sphere = header.sphere;
		desc.aabbMin = header.aabbMin;
		desc.aabbMax = header.aabbMax;
		desc.lodCount = header.lods.Size();
		desc.skinned = header.IsSkinned();

		// A skinned mesh keeps its skeleton loaded for as long as it is.
		if (desc.skinned)
		{
			LoadSkeleton(header.skeleton);
		}

		AssetData& assetData = m_AssetMap[ld->assetID];
		const MeshHandle meshHandle = m_RendererAPI->CreateMesh(desc);
		assetData.resourceHandle = meshHandle.h;
		// Marks the header available to other code. Geometry loading below doesn't need to
		// wait on anything else, but resolving pending mesh instances does wait on this being set.
		assetData.assetHeader = ld->header;
		assetData.pendingLodCount = header.lods.Size();
		// Nothing to wait for, such as a mesh file too old to read.
		if (header.lods.IsEmpty())
		{
			assetData.loadState = AssetLoadState::Loaded;
		}
		assetData.gpuSize = 0;
		for (const MeshChunkHeader& chunkHeader : header.chunks)
		{
			assetData.gpuSize += chunkHeader.decompressedVerticesSize + chunkHeader.decompressedIndicesSize + chunkHeader.decompressedMeshletsSize;
		}
		for (const MeshChunkSkin& chunkSkin : header.chunkSkins)
		{
			assetData.gpuSize += chunkSkin.decompressedSkinSize;
		}

		// Running byte offset into the file, starting right after the header - advances by
		// each chunk's compressed size as we walk them in the order they were written
		// (chunks sit back-to-back with no gaps, see MeshChunkHeader's comment).
		size_t fileOffset = ld->headerByteSize;

		for (uint chunkIndex = 0; chunkIndex < header.chunks.Size(); ++chunkIndex)
		{
			const MeshChunkHeader& chunkHeader = header.chunks[chunkIndex];

			// One chunk per LOD currently - find which LOD this one belongs to.
			uint lodIndex = 0;
			for (; lodIndex < header.lods.Size(); ++lodIndex)
			{
				if (header.lods[lodIndex].chunkOffset == chunkIndex)
				{
					break;
				}
			}

			MeshGeometryLoadData* geoLD = TempNew<MeshGeometryLoadData>();
			geoLD->assetID = ld->assetID;
			geoLD->mesh = meshHandle;
			geoLD->filePath = ld->filePath;
			geoLD->lodIndex = lodIndex;
			geoLD->fileOffset = fileOffset;
			geoLD->compressedBlobSize = chunkHeader.compressedBlobSize;
			geoLD->decompressedBlobSize = chunkHeader.decompressedBlobSize;
			geoLD->meshletsOffset = chunkHeader.meshletsOffset;
			geoLD->verticesOffset = chunkHeader.verticesOffset;
			geoLD->indicesOffset = chunkHeader.indicesOffset;
			geoLD->decompressedMeshletsSize = chunkHeader.decompressedMeshletsSize;
			geoLD->decompressedVerticesSize = chunkHeader.decompressedVerticesSize;
			geoLD->decompressedIndicesSize = chunkHeader.decompressedIndicesSize;
			if (desc.skinned)
			{
				geoLD->skinOffset = header.chunkSkins[chunkIndex].skinOffset;
				geoLD->decompressedSkinSize = header.chunkSkins[chunkIndex].decompressedSkinSize;
			}

			const uint meshletCount = chunkHeader.decompressedMeshletsSize / (uint)sizeof(MeshChunkMeshlet);

			// TODO: Retry if any allocation below is unsuccessful
			bool allocSuccess = m_RendererAPI->RequestResourceUploadAllocation(chunkHeader.decompressedVerticesSize, geoLD->verticesUpload);
			TYR_ASSERT(allocSuccess);
			allocSuccess = m_RendererAPI->RequestResourceUploadAllocation(chunkHeader.decompressedIndicesSize, geoLD->indicesUpload);
			TYR_ASSERT(allocSuccess);
			allocSuccess = m_RendererAPI->RequestResourceUploadAllocation(chunkHeader.decompressedMeshletsSize, geoLD->meshletsUpload);
			TYR_ASSERT(allocSuccess);

			allocSuccess = m_RendererAPI->RequestVertexBufferAllocation(meshHandle, lodIndex, chunkHeader.decompressedVerticesSize, geoLD->verticesGpuAlloc);
			TYR_ASSERT(allocSuccess);
			allocSuccess = m_RendererAPI->RequestIndexBufferAllocation(meshHandle, lodIndex, chunkHeader.decompressedIndicesSize, geoLD->indicesGpuAlloc);
			TYR_ASSERT(allocSuccess);
			allocSuccess = m_RendererAPI->RequestMeshletBufferAllocation(meshHandle, lodIndex, meshletCount * (uint)sizeof(ShaderMeshlet), geoLD->meshletsGpuAlloc);
			TYR_ASSERT(allocSuccess);
			if (geoLD->decompressedSkinSize > 0)
			{
				allocSuccess = m_RendererAPI->RequestResourceUploadAllocation(geoLD->decompressedSkinSize, geoLD->skinUpload);
				TYR_ASSERT(allocSuccess);
				allocSuccess = m_RendererAPI->RequestSkinVertexBufferAllocation(meshHandle, lodIndex, geoLD->decompressedSkinSize, geoLD->skinGpuAlloc);
				TYR_ASSERT(allocSuccess);
			}

			// The LOD descriptor doesn't need to wait on the async geometry load below - it
			// only needs the allocation offsets just requested above, already known here.
			GpuBufferAllocation meshLODGpuAlloc;
			allocSuccess = m_RendererAPI->RequestMeshLODBufferAllocation(meshHandle, lodIndex, meshLODGpuAlloc);
			TYR_ASSERT(allocSuccess);

			UploadBufferAllocation meshLODUpload;
			allocSuccess = m_RendererAPI->RequestResourceUploadAllocation(sizeof(ShaderMeshLOD), meshLODUpload);
			TYR_ASSERT(allocSuccess);

			ShaderMeshLOD* shaderMeshLOD = reinterpret_cast<ShaderMeshLOD*>(meshLODUpload.cpuPtr);
			// GpuBufferAllocation offsets are in bytes (shared buffers, byte-granular
			// allocators); ShaderMeshLOD's offsets are element indices, as read by shaders.
			shaderMeshLOD->vertexOffset = (uint)(geoLD->verticesGpuAlloc.offset / sizeof(ShaderVertex));
			shaderMeshLOD->indexOffset = (uint)(geoLD->indicesGpuAlloc.offset / sizeof(uint));
			shaderMeshLOD->meshletOffset = (uint)(geoLD->meshletsGpuAlloc.offset / sizeof(ShaderMeshlet));
			shaderMeshLOD->meshletCount = meshletCount;
			shaderMeshLOD->error = header.lods[lodIndex].error;

			m_RendererAPI->FlushBufferUploadAllocation(meshLODUpload);

			BufferUploadRequest meshLODRequest{};
			meshLODRequest.srcBuffer = meshLODUpload.buffer;
			meshLODRequest.srcOffset = meshLODUpload.offset;
			meshLODRequest.dstBuffer = meshLODGpuAlloc.buffer;
			meshLODRequest.dstOffset = meshLODGpuAlloc.offset;
			meshLODRequest.size = sizeof(ShaderMeshLOD);
			meshLODRequest.resourceId = meshLODUpload.resourceId;
			m_RendererAPI->AddBufferUploadRequest(meshLODRequest);

			fileOffset += chunkHeader.compressedBlobSize;

			// Reads this chunk's compressed bytes, decompresses them off the main thread, and
			// copies each region straight into its already-reserved upload allocation.
			TaskScheduler::Instance().CreateAndEnqueueTask([this, geoLD]()
			{
				char absFilePath[TYR_MAX_PATH_TOTAL_SIZE];
				AssetUtil::CreateFullPath(absFilePath, geoLD->filePath.CStr());

				SmartStack<uint8> compressedStack = SmartStackAlloc<uint8>(geoLD->compressedBlobSize);
				uint8* compressed = compressedStack;
				{
					FileStream fileStream(absFilePath);
					fileStream.Seek(geoLD->fileOffset);
					fileStream.Read(compressed, geoLD->compressedBlobSize);
				}

				SmartStack<uint8> decompressedStack = SmartStackAlloc<uint8>(geoLD->decompressedBlobSize);
				uint8* decompressed = decompressedStack;
				const size_t decompressedSize = ZSTD_decompress(decompressed, geoLD->decompressedBlobSize, compressed, geoLD->compressedBlobSize);
				TYR_ASSERT(!ZSTD_isError(decompressedSize) && decompressedSize == geoLD->decompressedBlobSize);

				// Indices are stored per meshlet. Making them relative to the LOD's first vertex lets
				// ray tracing build from them too, and the mesh shader takes the meshlet's offset back off.
				const MeshChunkMeshlet* meshlets = reinterpret_cast<const MeshChunkMeshlet*>(decompressed + geoLD->meshletsOffset);
				uint* indices = reinterpret_cast<uint*>(decompressed + geoLD->indicesOffset);
				const uint meshletCount = geoLD->decompressedMeshletsSize / (uint)sizeof(MeshChunkMeshlet);
				for (uint m = 0; m < meshletCount; ++m)
				{
					const MeshChunkMeshlet& meshlet = meshlets[m];
					for (uint i = meshlet.indexOffset; i < meshlet.indexOffset + meshlet.indexCount; ++i)
					{
						indices[i] += meshlet.vertexOffset;
					}
				}

				memcpy(geoLD->verticesUpload.cpuPtr, decompressed + geoLD->verticesOffset, geoLD->decompressedVerticesSize);
				memcpy(geoLD->indicesUpload.cpuPtr, decompressed + geoLD->indicesOffset, geoLD->decompressedIndicesSize);
				memcpy(geoLD->meshletsUpload.cpuPtr, decompressed + geoLD->meshletsOffset, geoLD->decompressedMeshletsSize);
				if (geoLD->decompressedSkinSize > 0)
				{
					memcpy(geoLD->skinUpload.cpuPtr, decompressed + geoLD->skinOffset, geoLD->decompressedSkinSize);
				}

				m_MeshesLoadedQueue.Enqueue(geoLD);
			});
		}

		TempDelete<MeshHeaderLoadData>(ld);
		return true;
	}

	void AssetManager::UploadMeshGeometry(MeshGeometryLoadData* ld)
	{
		// No CPU-side transform needed - the task already decompressed straight into
		// ld->meshletsUpload in its final, GPU-ready form, same as vertices/indices.
		const uint meshletCount = ld->decompressedMeshletsSize / (uint)sizeof(ShaderMeshlet);

		m_RendererAPI->FlushBufferUploadAllocation(ld->verticesUpload);
		m_RendererAPI->FlushBufferUploadAllocation(ld->indicesUpload);
		m_RendererAPI->FlushBufferUploadAllocation(ld->meshletsUpload);

		BufferUploadRequest verticesRequest{};
		verticesRequest.srcBuffer = ld->verticesUpload.buffer;
		verticesRequest.srcOffset = ld->verticesUpload.offset;
		verticesRequest.dstBuffer = ld->verticesGpuAlloc.buffer;
		verticesRequest.dstOffset = ld->verticesGpuAlloc.offset;
		verticesRequest.size = ld->decompressedVerticesSize;
		verticesRequest.resourceId = ld->verticesUpload.resourceId;
		m_RendererAPI->AddBufferUploadRequest(verticesRequest);

		BufferUploadRequest indicesRequest{};
		indicesRequest.srcBuffer = ld->indicesUpload.buffer;
		indicesRequest.srcOffset = ld->indicesUpload.offset;
		indicesRequest.dstBuffer = ld->indicesGpuAlloc.buffer;
		indicesRequest.dstOffset = ld->indicesGpuAlloc.offset;
		indicesRequest.size = ld->decompressedIndicesSize;
		indicesRequest.resourceId = ld->indicesUpload.resourceId;
		m_RendererAPI->AddBufferUploadRequest(indicesRequest);

		BufferUploadRequest meshletsRequest{};
		meshletsRequest.srcBuffer = ld->meshletsUpload.buffer;
		meshletsRequest.srcOffset = ld->meshletsUpload.offset;
		meshletsRequest.dstBuffer = ld->meshletsGpuAlloc.buffer;
		meshletsRequest.dstOffset = ld->meshletsGpuAlloc.offset;
		meshletsRequest.size = meshletCount * (uint)sizeof(ShaderMeshlet);
		meshletsRequest.resourceId = ld->meshletsUpload.resourceId;
		m_RendererAPI->AddBufferUploadRequest(meshletsRequest);

		const bool skinned = ld->decompressedSkinSize > 0;
		if (skinned)
		{
			m_RendererAPI->FlushBufferUploadAllocation(ld->skinUpload);

			BufferUploadRequest skinRequest{};
			skinRequest.srcBuffer = ld->skinUpload.buffer;
			skinRequest.srcOffset = ld->skinUpload.offset;
			skinRequest.dstBuffer = ld->skinGpuAlloc.buffer;
			skinRequest.dstOffset = ld->skinGpuAlloc.offset;
			skinRequest.size = ld->decompressedSkinSize;
			skinRequest.resourceId = ld->skinUpload.resourceId;
			m_RendererAPI->AddBufferUploadRequest(skinRequest);
		}

		// Ray-traced shadows only ever use LOD0 - queue its one-time BLAS build as soon as its
		// vertex/index upload above is queued, since the build's ordering against that upload
		// relies on both being requested in the same or an earlier frame. Skinned instances
		// build their own.
		if (ld->lodIndex == 0 && !skinned)
		{
			m_RendererAPI->RequestBLASBuild(ld->mesh);
		}

		// All LODs currently start loading together (see CreateMesh), so this just needs to
		// count down as each one finishes - the mesh is Loaded once every LOD is in.
		AssetData& assetData = m_AssetMap[ld->assetID];
		TYR_ASSERT(assetData.pendingLodCount > 0);
		if (--assetData.pendingLodCount == 0)
		{
			assetData.loadState = AssetLoadState::Loaded;
		}

		TempDelete<MeshGeometryLoadData>(ld);
	}

	void AssetManager::LoadMeshInstanceAssets(AssetID meshAssetID, const LocalArray<AssetID, MeshConstants::c_MaxSubmeshes>& materials, bool skinned, Function<void(const MeshInstanceAssets&)> onLoaded)
	{
		LoadMesh(meshAssetID);

		MeshInstanceAssetsRequest* request = TempNew<MeshInstanceAssetsRequest>();
		request->meshAssetID = meshAssetID;
		request->materials = materials;
		request->onLoaded = std::move(onLoaded);
		request->skinned = skinned;
		m_PendingMeshInstanceAssets.Add(request);
	}

	AssetID AssetManager::GetEffectiveMaterialForSlot(const MeshHeader& header, const MeshInstanceAssetsRequest& request, uint slot) const
	{
		if (slot < request.materials.Size() && AssetUtil::IsValidAssetID(request.materials[slot]))
		{
			return request.materials[slot];
		}
		return header.materials[slot];
	}

	void AssetManager::TryResolvePendingMeshInstanceAssets()
	{
		for (uint i = 0; i < m_PendingMeshInstanceAssets.Size();)
		{
			MeshInstanceAssetsRequest* request = m_PendingMeshInstanceAssets[i];
			const AssetData& meshAssetData = m_AssetMap[request->meshAssetID];

			bool ready = false;

			// The header has to be available before anything else here can happen - it's
			// what says how many submesh slots there are and what their defaults are.
			if ((bool)meshAssetData.assetHeader)
			{
				const MeshHeader& header = m_MeshHeaderPool[meshAssetData.assetHeader];
				TYR_ASSERT(header.materials.Size() <= MeshConstants::c_MaxSubmeshes);

				if (!request->materialsRequested)
				{
					// Every slot needs *something* loaded - either its override or the
					// mesh's own default for that slot. Only safe to work out now that the
					// header (and so the default list) is actually available.
					for (uint slot = 0; slot < header.materials.Size(); ++slot)
					{
						LoadMaterial(GetEffectiveMaterialForSlot(header, *request, slot));
					}
					request->materialsRequested = true;
				}

				// A skinned instance is skinned from the uploaded geometry by its skeleton.
				ready = !request->skinned || (meshAssetData.loadState == AssetLoadState::Loaded && GetSkeleton(header.skeleton));
				for (uint slot = 0; ready && slot < header.materials.Size(); ++slot)
				{
					const AssetID materialID = GetEffectiveMaterialForSlot(header, *request, slot);
					if (m_AssetMap[materialID].loadState != AssetLoadState::Loaded)
					{
						ready = false;
						break;
					}
				}
			}

			if (ready)
			{
				// Taken off the list first, since the callback can request more.
				m_PendingMeshInstanceAssets.SwapAndPopBack(i);
				FinishMeshInstanceAssets(request);
			}
			else
			{
				++i;
			}
		}
	}

	void AssetManager::FinishMeshInstanceAssets(MeshInstanceAssetsRequest* request)
	{
		const AssetData& meshAssetData = m_AssetMap[request->meshAssetID];
		const MeshHeader& header = m_MeshHeaderPool[meshAssetData.assetHeader];

		MeshInstanceAssets assets;
		assets.mesh = MeshHandle(meshAssetData.resourceHandle);
		for (uint slot = 0; slot < header.materials.Size(); ++slot)
		{
			const AssetID materialID = GetEffectiveMaterialForSlot(header, *request, slot);
			assets.materials.Add(MaterialHandle(m_AssetMap[materialID].resourceHandle));
			assets.materialIDs.Add(materialID);
		}

		if (request->onLoaded)
		{
			request->onLoaded(assets);
		}
		TempDelete<MeshInstanceAssetsRequest>(request);
	}

	void AssetManager::LoadSkeleton(AssetID assetID)
	{
		LoadCpuAsset(assetID, AssetLoadType::Skeleton);
	}

	void AssetManager::DeleteSkeleton(AssetID assetID)
	{
		DeleteCpuAsset(assetID, AssetLoadType::Skeleton);
	}

	const Skeleton* AssetManager::GetSkeleton(AssetID assetID) const
	{
		const AssetData* assetData = m_AssetMap.Find(assetID);
		return assetData && assetData->loadState == AssetLoadState::Loaded ? &m_SkeletonPool[assetData->resourceHandle] : nullptr;
	}

	void AssetManager::LoadAnimation(AssetID assetID)
	{
		LoadCpuAsset(assetID, AssetLoadType::Animation);
	}

	void AssetManager::DeleteAnimation(AssetID assetID)
	{
		DeleteCpuAsset(assetID, AssetLoadType::Animation);
	}

	const AnimationClip* AssetManager::GetAnimation(AssetID assetID) const
	{
		const AssetData* assetData = m_AssetMap.Find(assetID);
		return assetData && assetData->loadState == AssetLoadState::Loaded ? &m_AnimationPool[assetData->resourceHandle] : nullptr;
	}

	void AssetManager::LoadCollisionMesh(AssetID assetID)
	{
		LoadCpuAsset(assetID, AssetLoadType::CollisionMesh);
	}

	void AssetManager::DeleteCollisionMesh(AssetID assetID)
	{
		DeleteCpuAsset(assetID, AssetLoadType::CollisionMesh);
	}

	const CollisionMesh* AssetManager::GetCollisionMesh(AssetID assetID) const
	{
		const AssetData* assetData = m_AssetMap.Find(assetID);
		return assetData && assetData->loadState == AssetLoadState::Loaded ? &m_CollisionMeshPool[assetData->resourceHandle] : nullptr;
	}

	void AssetManager::LoadCpuAsset(AssetID assetID, AssetLoadType type)
	{
		if (AssetData* assetData = m_AssetMap.Find(assetID))
		{
			assetData->refCount++;
			return;
		}

		AssetData& asset = m_AssetMap[assetID];
		asset.loadState = AssetLoadState::Loading;
		asset.refCount = 1;

		CpuAssetLoadData* ld = TempNew<CpuAssetLoadData>();
		ld->assetID = assetID;
		ld->filePath = AssetRegistry::Instance().GetAssetData(assetID).filePath;
		ld->type = type;
		// Pool slots are only made and freed on the main thread.
		switch (type)
		{
		case AssetLoadType::Skeleton:
			ld->handle = m_SkeletonPool.Create();
			break;
		case AssetLoadType::Animation:
			ld->handle = m_AnimationPool.Create();
			break;
		default:
			ld->handle = m_CollisionMeshPool.Create();
			break;
		}
		asset.resourceHandle = ld->handle;

		TaskScheduler::Instance().CreateAndEnqueueTask([this, ld]()
		{
			switch (ld->type)
			{
			case AssetLoadType::Skeleton:
				LoadAssetFile(ld->filePath.CStr(), m_SkeletonPool[ld->handle]);
				break;
			case AssetLoadType::Animation:
				LoadAssetFile(ld->filePath.CStr(), m_AnimationPool[ld->handle]);
				break;
			default:
				LoadAssetFile(ld->filePath.CStr(), m_CollisionMeshPool[ld->handle]);
				break;
			}
			m_CpuAssetsLoadedQueue.Enqueue(ld);
		});
	}

	void AssetManager::DeleteCpuAsset(AssetID assetID, AssetLoadType type)
	{
		AssetData* assetData = m_AssetMap.Find(assetID);
		TYR_ASSERT(assetData);
		if (assetData->refCount > 1)
		{
			assetData->refCount--;
			return;
		}

		assetData->refCount = 0;
		// One still loading is freed once its load finishes, unless it's loaded again first.
		if (assetData->loadState == AssetLoadState::Loaded)
		{
			FreeCpuAsset(assetID, *assetData, type);
		}
	}

	void AssetManager::FreeCpuAsset(AssetID assetID, const AssetData& assetData, AssetLoadType type)
	{
		switch (type)
		{
		case AssetLoadType::Skeleton:
			m_SkeletonPool.Delete(assetData.resourceHandle);
			break;
		case AssetLoadType::Animation:
			m_AnimationPool.Delete(assetData.resourceHandle);
			break;
		default:
			m_CollisionMeshPool.Delete(assetData.resourceHandle);
			break;
		}
		m_AssetMap.Erase(assetID);
	}

	void AssetManager::FinishCpuAssetLoad(CpuAssetLoadData* ld)
	{
		AssetData& assetData = m_AssetMap[ld->assetID];
		assetData.loadState = AssetLoadState::Loaded;
		if (assetData.refCount == 0)
		{
			FreeCpuAsset(ld->assetID, assetData, ld->type);
		}
		TempDelete<CpuAssetLoadData>(ld);
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
