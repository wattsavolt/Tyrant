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
		m_PendingMeshInstances.Reserve(100);
		m_PendingTextureDeletes.Reserve(16);
		m_PendingMaterialDeletes.Reserve(16);
		m_PendingMeshDeletes.Reserve(16);

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
		while (Optional<TextureHeaderLoadData*> ld = m_TextureHeadersLoadedQueue.Dequeue())
		{
			CreateTexture(ld.value());
		}

		while (Optional<TexturePixelLoadData*> ld = m_TexturesLoadedQueue.Dequeue())
		{
			UploadTexture(ld.value());
		}

		while (Optional<MaterialLoadData*> ld = m_MaterialFilesLoadedQueue.Dequeue())
		{
			ResolveMaterialTextures(ld.value());
		}

		while (Optional<MeshHeaderLoadData*> ld = m_MeshHeadersLoadedQueue.Dequeue())
		{
			CreateMesh(ld.value());
		}

		while (Optional<MeshGeometryLoadData*> ld = m_MeshesLoadedQueue.Dequeue())
		{
			UploadMeshGeometry(ld.value());
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

		TryResolvePendingMeshInstances();

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

		while (Optional<MeshGeometryLoadData*> ld = m_MeshesLoadedQueue.Dequeue())
		{
			TempDelete<MeshGeometryLoadData>(ld.value());
		}

		for (MaterialLoadData* ld : m_MaterialsAwaitingTextures)
		{
			TempDelete<MaterialLoadData>(ld);
		}
		m_MaterialsAwaitingTextures.Clear();

		for (MeshInstanceCreateData* instData : m_PendingMeshInstances)
		{
			TempDelete<MeshInstanceCreateData>(instData);
		}
		m_PendingMeshInstances.Clear();
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

	void AssetManager::DeleteTexture(AssetID assetID)
	{
		AssetData* assetData = m_AssetMap.Find(assetID);
		TYR_ASSERT(assetData);
		if (assetData->refCount <= 1)
		{
			assetData->refCount = 0;
			if (assetData->loadState == AssetLoadState::Loaded)
			{
				DeleteTextureResources(assetID, *assetData);
			}
			else
			{
				// Still loading (pixel data not read in yet) - the main-thread follow-up reads
				// this same AssetData and the renderer's pool slot, so freeing either now
				// could race with it.
				m_PendingTextureDeletes.Add(assetID);
			}
		}
		else
		{
			assetData->refCount--;
		}
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
			const AssetID assetID = m_PendingTextureDeletes[i];
			AssetData* assetData = m_AssetMap.Find(assetID);
			// A missing entry, or a refCount that's no longer 0, means something else (a fresh
			// Load* call) already resolved this one way or another - nothing left to do here.
			if (!assetData || assetData->refCount != 0)
			{
				m_PendingTextureDeletes.SwapAndPopBack(i);
				continue;
			}

			if (assetData->loadState == AssetLoadState::Loaded)
			{
				DeleteTextureResources(assetID, *assetData);
				m_PendingTextureDeletes.SwapAndPopBack(i);
			}
			else
			{
				++i;
			}
		}
	}

	void AssetManager::CreateTexture(TextureHeaderLoadData* ld)
	{
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

		TexturePixelLoadData* pixelLD = TempNew<TexturePixelLoadData>();
		pixelLD->assetID = ld->assetID;
		pixelLD->texture = textureHandle;
		pixelLD->filePath = ld->filePath;

		// TODO: Retry if allocation is unsuccessful
		const bool allocSuccess = m_RendererAPI->RequestResourceUploadAllocation(ld->header.dataSize, pixelLD->allocation);
		TYR_ASSERT(allocSuccess);

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

	void AssetManager::DeleteMaterial(AssetID assetID)
	{
		AssetData* assetData = m_AssetMap.Find(assetID);
		TYR_ASSERT(assetData);
		if (assetData->refCount <= 1)
		{
			assetData->refCount = 0;
			if (assetData->loadState == AssetLoadState::Loaded)
			{
				DeleteMaterialResources(assetID, *assetData);
			}
			else
			{
				// Still loading (file parse/texture-dependency resolution in flight) - the
				// renderer resource doesn't exist yet, and tearing down now could race with
				// the main-thread follow-up still reading each texture dependency's AssetData.
				m_PendingMaterialDeletes.Add(assetID);
			}
		}
		else
		{
			assetData->refCount--;
		}
	}

	void AssetManager::DeleteMaterialResources(AssetID assetID, AssetData& assetData)
	{
		uint depCount;
		const AssetID* dependencies = AssetRegistry::Instance().GetAssetDependencies(assetID, depCount);
		for (uint i = 0; i < depCount; ++i)
		{
			DeleteTexture(dependencies[i]);
		}
		m_RendererAPI->DeleteMaterial(MaterialHandle(assetData.resourceHandle));
		m_AssetMap.Erase(assetID);
	}

	void AssetManager::ProcessPendingMaterialDeletes()
	{
		for (uint i = 0; i < m_PendingMaterialDeletes.Size();)
		{
			const AssetID assetID = m_PendingMaterialDeletes[i];
			AssetData* assetData = m_AssetMap.Find(assetID);
			if (!assetData || assetData->refCount != 0)
			{
				m_PendingMaterialDeletes.SwapAndPopBack(i);
				continue;
			}

			if (assetData->loadState == AssetLoadState::Loaded)
			{
				DeleteMaterialResources(assetID, *assetData);
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
			ld->filePath = regAssetData.filePath;

			// Pool create/destroy has to stay on the thread that owns the pool (main) - only
			// reading/writing the header's already-allocated slot is safe from the task below.
			ld->header = m_MeshHeaderPool.Create();

			TaskScheduler::Instance().CreateAndEnqueueTask([this, ld]()
			{
				char absFilePath[TYR_MAX_PATH_TOTAL_SIZE];
				AssetUtil::CreateFullPath(absFilePath, ld->filePath.CStr());

				constexpr size_t c_StreamBufferSize = 65536;
				SmartStack<uint8> streamBufferStack = SmartStackAlloc<uint8>((uint)c_StreamBufferSize);
				BufferedFileStream stream(streamBufferStack, c_StreamBufferSize, absFilePath, BinaryStream::Operation::Read);

				MeshHeader& header = m_MeshHeaderPool[ld->header];
				Deserialize<MeshHeader>(stream, header);
				// Exact on-disk header size, from the stream's position right after reading
				// it - lets CreateMesh compute each chunk's absolute file offset below
				// without the file needing to store them (see MeshChunkHeader's comment).
				ld->headerByteSize = stream.GetOffset();

				m_MeshHeadersLoadedQueue.Enqueue(ld);
			});
		}
	}

	void AssetManager::DeleteMesh(AssetID assetID)
	{
		AssetData* assetData = m_AssetMap.Find(assetID);
		TYR_ASSERT(assetData);
		if (assetData->refCount <= 1)
		{
			assetData->refCount = 0;
			if (assetData->loadState == AssetLoadState::Loaded)
			{
				DeleteMeshResources(assetID, *assetData);
			}
			else
			{
				// Still loading (header and/or LOD geometry not finished yet) - the main-thread
				// follow-up still reads/writes this same AssetData, and freeing the header pool
				// slot now would leave it working with a deleted (or since-reused) slot.
				m_PendingMeshDeletes.Add(assetID);
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
		m_MeshHeaderPool.Delete(assetData.meshHeader);
		m_AssetMap.Erase(assetID);
	}

	void AssetManager::ProcessPendingMeshDeletes()
	{
		for (uint i = 0; i < m_PendingMeshDeletes.Size();)
		{
			const AssetID assetID = m_PendingMeshDeletes[i];
			AssetData* assetData = m_AssetMap.Find(assetID);
			if (!assetData || assetData->refCount != 0)
			{
				m_PendingMeshDeletes.SwapAndPopBack(i);
				continue;
			}

			if (assetData->loadState == AssetLoadState::Loaded)
			{
				DeleteMeshResources(assetID, *assetData);
				m_PendingMeshDeletes.SwapAndPopBack(i);
			}
			else
			{
				++i;
			}
		}
	}

	void AssetManager::CreateMesh(MeshHeaderLoadData* ld)
	{
		MeshHeader& header = m_MeshHeaderPool[ld->header];

		MeshDesc desc;
		desc.sphere = header.sphere;
		desc.aabbMin = header.aabbMin;
		desc.aabbMax = header.aabbMax;
		desc.lodCount = header.lods.Size();

		AssetData& assetData = m_AssetMap[ld->assetID];
		const MeshHandle meshHandle = m_RendererAPI->CreateMesh(desc);
		assetData.resourceHandle = meshHandle.h;
		// Marks the header available to other code - see AssetData::meshHeader's comment.
		// Geometry loading below doesn't need to wait on anything else, but
		// TryResolvePendingMeshInstances does wait on this being set.
		assetData.meshHeader = ld->header;
		assetData.pendingLodCount = header.lods.Size();

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

				memcpy(geoLD->verticesUpload.cpuPtr, decompressed + geoLD->verticesOffset, geoLD->decompressedVerticesSize);
				memcpy(geoLD->indicesUpload.cpuPtr, decompressed + geoLD->indicesOffset, geoLD->decompressedIndicesSize);
				memcpy(geoLD->meshletsUpload.cpuPtr, decompressed + geoLD->meshletsOffset, geoLD->decompressedMeshletsSize);

				m_MeshesLoadedQueue.Enqueue(geoLD);
			});
		}

		TempDelete<MeshHeaderLoadData>(ld);
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

		// Ray-traced shadows only ever use LOD0 - queue its one-time BLAS build as soon as its
		// vertex/index upload above is queued, since the build's ordering against that upload
		// relies on both being requested in the same or an earlier frame.
		if (ld->lodIndex == 0)
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

	void AssetManager::CreateMeshInstance(AssetID meshAssetID, const Matrix4& transform, const LocalArray<MaterialOverride, MeshConstants::c_MaxSubmeshes>& overrides, Function<void(MeshInstanceHandle, const LocalArray<AssetID, MeshConstants::c_MaxSubmeshes>&)> onCreated)
	{
		LoadMesh(meshAssetID);

		MeshInstanceCreateData* instData = TempNew<MeshInstanceCreateData>();
		instData->meshAssetID = meshAssetID;
		instData->transform = transform;
		instData->overrides = overrides;
		instData->onCreated = std::move(onCreated);
		m_PendingMeshInstances.Add(instData);
	}

	AssetID AssetManager::GetEffectiveMaterialForSlot(const MeshHeader& header, const MeshInstanceCreateData& instData, uint slot) const
	{
		for (const MaterialOverride& override : instData.overrides)
		{
			if (override.submeshSlot == slot)
			{
				return override.material;
			}
		}
		return header.materials[slot];
	}

	void AssetManager::TryResolvePendingMeshInstances()
	{
		for (uint i = 0; i < m_PendingMeshInstances.Size();)
		{
			MeshInstanceCreateData* instData = m_PendingMeshInstances[i];
			const AssetData& meshAssetData = m_AssetMap[instData->meshAssetID];

			bool ready = false;

			// The header has to be available before anything else here can happen - it's
			// what says how many submesh slots there are and what their defaults are.
			if ((bool)meshAssetData.meshHeader)
			{
				const MeshHeader& header = m_MeshHeaderPool[meshAssetData.meshHeader];
				TYR_ASSERT(header.materials.Size() <= MeshConstants::c_MaxSubmeshes);

				if (!instData->materialsRequested)
				{
					// Every slot needs *something* loaded - either its override or the
					// mesh's own default for that slot. Only safe to work out now that the
					// header (and so the default list) is actually available.
					for (uint slot = 0; slot < header.materials.Size(); ++slot)
					{
						LoadMaterial(GetEffectiveMaterialForSlot(header, *instData, slot));
					}
					instData->materialsRequested = true;
				}

				ready = true;
				for (uint slot = 0; slot < header.materials.Size(); ++slot)
				{
					const AssetID materialID = GetEffectiveMaterialForSlot(header, *instData, slot);
					if (m_AssetMap[materialID].loadState != AssetLoadState::Loaded)
					{
						ready = false;
						break;
					}
				}
			}

			if (ready)
			{
				CreateResolvedMeshInstance(instData);
				m_PendingMeshInstances.SwapAndPopBack(i);
			}
			else
			{
				++i;
			}
		}
	}

	void AssetManager::CreateResolvedMeshInstance(MeshInstanceCreateData* instData)
	{
		const AssetData& meshAssetData = m_AssetMap[instData->meshAssetID];
		const MeshHeader& header = m_MeshHeaderPool[meshAssetData.meshHeader];

		MeshInstanceDesc desc;
		desc.info.transform = instData->transform;
		desc.info.mesh = MeshHandle(meshAssetData.resourceHandle);
		desc.info.materials.Resize(header.materials.Size());
		LocalArray<AssetID, MeshConstants::c_MaxSubmeshes> materialAssetIDs;
		for (uint slot = 0; slot < header.materials.Size(); ++slot)
		{
			const AssetID materialID = GetEffectiveMaterialForSlot(header, *instData, slot);
			desc.info.materials[slot] = MaterialHandle(m_AssetMap[materialID].resourceHandle);
			materialAssetIDs.Add(materialID);
		}

		const MeshInstanceHandle handle = m_RendererAPI->CreateMeshInstance(desc);
		if (instData->onCreated)
		{
			instData->onCreated(handle, materialAssetIDs);
		}

		TempDelete<MeshInstanceCreateData>(instData);
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
