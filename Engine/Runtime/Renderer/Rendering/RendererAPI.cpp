#include "RendererAPI.h"
#include "RenderDebug.h"
#include "Renderer.h"
#include "RenderResource/RenderResourceUtil.h"
#include "RenderAPI/Device.h"
#include "RenderRegistry.h"
#include "RenderTransfer/RenderTransferTypes.h"
#include "GUIDrawData.h"

namespace tyr
{
	RendererAPI::RendererAPI(Renderer& renderer)
		: m_Renderer(renderer)
		, m_Registry(*RenderRegistry::Instance())
		, m_AllocManager(m_Renderer.GetAllocationManager())
		, m_Device(m_Registry.GetDevice())
	{
		
	}

	RendererAPI::~RendererAPI()
	{
	
	}

	RenderWindowHandle RendererAPI::AddWindow(void* osHandle)
	{
		return m_Renderer.AddWindow(osHandle);
	}

	void RendererAPI::RemoveWindow(RenderWindowHandle window)
	{
		m_Renderer.RemoveWindow(window);
	}

	void RendererAPI::ResizeWindow(RenderWindowHandle window)
	{
		m_Renderer.ResizeWindow(window);
	}

	SceneHandle RendererAPI::AddScene(const char* name)
	{
		// It's okay to do these modifications to the render data on this thread as any async thread doing rendering work
		// would only be modifying the active scene which won't be touched here
		RenderData& data = m_Renderer.GetRenderData();
		static uint nextID = 0;
		const SceneHandle handle(data.scenePool.Create());
		Scene& scene = data.scenePool[handle.h];
		scene.name = name;
		scene.id = nextID++;
		// Covers pool-slot reuse - see ImmediateSceneData's own comment.
		m_Renderer.GetImmediateSceneData(handle).Reset();
		return handle;
	}

	void RendererAPI::RemoveScene(SceneHandle handle)
	{
		// The scene's data can't be reset right here - RenderAsync might still be reading/
		// writing it from up to c_BufferedFrameCount frames ago. Stopping it being assigned as
		// the active scene from this point on (below) means no *new* task will ever be created
		// that still references it, so every task that could still be using it is one of the
		// (at most c_BufferedFrameCount) already created - and since tasks are strictly
		// serialized, all of those are guaranteed done once this cycles back around, the same
		// bound RemoveWindow's deferred deletion relies on. PrepareForNextFrame processes this
		// list once that's confirmed, or the destructor force-flushes it at shutdown.
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		if (renderFrame.activeScene == handle)
		{
			renderFrame.activeScene = {};
		}
		renderFrame.scenesToDelete.Add(handle);
	}

	void RendererAPI::SetActiveScene(SceneHandle handle, bool visible)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		renderFrame.activeScene = handle;
		m_Renderer.GetImmediateSceneData(handle).visible = visible;
	}

	RenderViewportHandle RendererAPI::CreateRenderViewport()
	{
		return m_Renderer.CreateRenderViewport();
	}

	void RendererAPI::DeleteRenderViewport(RenderViewportHandle viewport)
	{
		m_Renderer.DeleteRenderViewport(viewport);
	}

	void RendererAPI::SetSceneRenderViewport(SceneHandle scene, RenderViewportHandle viewport)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		renderFrame.sceneFrame.newRenderViewport = viewport;
		m_Renderer.GetImmediateSceneData(scene).renderViewport = viewport;
	}

	void RendererAPI::AddBufferUploadRequest(const BufferUploadRequest& request)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		renderFrame.assetBufferUploadRequests.Add(request);
	}

	void RendererAPI::AddTextureUploadRequest(const TextureUploadRequest& request)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		renderFrame.textureUploadRequests.Add(request);
	}

	TextureHandle RendererAPI::CreateTexture(const TextureDesc& desc)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		const TextureHandle handle = m_Registry.CreateTexture(desc);
		renderFrame.texturesToAdd.Add(handle);

		// A texture's own pool index doubles as its slot in the bindless textures[] array -
		// ShaderMaterial::texture0 etc already store that same index.
		const Texture& texture = m_Registry.GetTexture(handle);
		ImageBindingInfo imageInfo;
		imageInfo.imageView = texture.imageView;
		imageInfo.hasSampler = false;
		// Must match whatever layout this texture is actually kept in (see UploadToTextures'
		// barrier, which transitions to and leaves it at texture.imageLayout) - a mismatch here
		// is invalid descriptor usage and reads back as garbage/black regardless of how correct
		// the underlying image data is.
		imageInfo.layout = texture.imageLayout;

		// Queued rather than written immediately - see PendingDescriptorUpdates' own comment.
		m_Renderer.QueueImageBindingUpdate(TYR_BINDING_TEXTURES, handle.h.index, imageInfo);

		return handle;
	}

	void RendererAPI::DeleteTexture(TextureHandle handle)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		renderFrame.texturesToDelete.Add(handle);
	}

	const TextureInfo& RendererAPI::GetTextureInfo(TextureHandle handle)
	{
		return m_Registry.GetTexture(handle).info;
	}

	TextureHandle RendererAPI::GetOrCreateRenderViewportTexture(RenderViewportHandle viewport, const char* name, uint width, uint height)
	{
		LocalObjectPool<RenderViewport, RenderConstants::c_MaxScenes>& pool = m_Renderer.GetRenderViewportPool();
		RenderViewport& rv = pool[viewport.h];

		// Only ever (re)creates this tick's own slot's targets - the other c_BufferedFrameCount-1
		// slots keep whatever they already have until their own turn comes back around (stable for
		// the whole tick - see GetRenderFrameIndex's own comment). Main thread only, like every
		// RendererAPI Create*/Delete* call - RecordGeometryPass/RecordLightingPass (a worker thread)
		// only ever read their own tick's slot, so nothing else touches it concurrently here.
		const uint renderFrameIndex = m_Renderer.GetRenderFrameIndex();
		const RenderViewportTextureData& current = rv.textureData[renderFrameIndex];
		if (!current.colourTexture || current.width != width || current.height != height)
		{
			m_Renderer.ResizeRenderViewportSlot(rv, renderFrameIndex, name, width, height);
		}

		// Record the latest requested size and flag the other two buffered slots to pick it up on
		// their own next turn - see RenderViewport's own comment on resize propagation.
		rv.requestedWidth = width;
		rv.requestedHeight = height;
		for (uint i = 1; i < RenderConstants::c_BufferedFrameCount; ++i)
		{
			const uint otherSlot = (renderFrameIndex + i) % RenderConstants::c_BufferedFrameCount;
			rv.pendingResize[otherSlot] = true;
		}

		return rv.textureData[renderFrameIndex].colourTexture;
	}

	MaterialHandle RendererAPI::CreateMaterial(const MaterialDesc& desc)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		RenderResources& resources = m_Renderer.GetRenderResources();

		const MaterialHandle handle = m_Registry.CreateMaterial(desc);
		const Material& material = m_Registry.GetMaterial(handle);

		ShaderMaterial shaderMaterial;

		if (material.type == MaterialType::PBR)
		{
			shaderMaterial.texture0 = material.textures[MaterialConstants::c_PbrAlbedoIndex].index;
			shaderMaterial.texture1 = material.textures[MaterialConstants::c_PbrNormalHeightIndex].index;
			shaderMaterial.texture2 = material.textures[MaterialConstants::c_PbrAoRoughnessMetallicIndex].index;
			shaderMaterial.flags = 0;
		}

		UploadBufferAllocation alloc;
		const bool uploadSuccess = m_AllocManager.RequestFrameUploadAllocation(sizeof(ShaderMaterial), alloc);
		TYR_ASSERT(uploadSuccess);

		if (uploadSuccess)
		{
			RenderResourceUtil::WriteUploadBuffer(m_Registry.GetBuffer(alloc.buffer), m_Device, alloc.offset, &shaderMaterial, sizeof(ShaderMaterial));

			BufferUploadRequest& request = renderFrame.frameBufferUploadRequests.ExpandOne();
			request.srcBuffer = alloc.buffer;
			request.srcOffset = alloc.offset;
			request.dstBuffer = resources.materialBuffer;
			request.dstOffset = sizeof(ShaderMaterial) * handle.h.index;
			request.size = sizeof(ShaderMaterial);
		}

		return handle;
	}

	void RendererAPI::DeleteMaterial(MaterialHandle handle)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		renderFrame.materialsToDelete.Add(handle);
	}

	MeshHandle RendererAPI::CreateMesh(const MeshDesc& desc)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		RenderResources& resources = m_Renderer.GetRenderResources();

		const uint lodOffset = m_AllocManager.AllocateMeshLODs();
		const MeshHandle handle = m_Registry.CreateMesh(desc, lodOffset);
		const Mesh& mesh = m_Registry.GetMesh(handle);

		ShaderMesh shaderMesh;
		shaderMesh.sphere = Vector4(mesh.sphere.m_Centre.x, mesh.sphere.m_Centre.y, mesh.sphere.m_Centre.z, mesh.sphere.m_Radius);
		shaderMesh.aabbMin = mesh.aabbMin;
		shaderMesh.aabbMax = mesh.aabbMax;
		shaderMesh.lodOffset = mesh.lodOffset;
		shaderMesh.lodCount = mesh.lodCount;

		UploadBufferAllocation alloc;
		const bool uploadSuccess = m_AllocManager.RequestFrameUploadAllocation(sizeof(ShaderMesh), alloc);
		TYR_ASSERT(uploadSuccess);

		if (uploadSuccess)
		{
			RenderResourceUtil::WriteUploadBuffer(m_Registry.GetBuffer(alloc.buffer), m_Device, alloc.offset, &shaderMesh, sizeof(ShaderMesh));

			BufferUploadRequest& request = renderFrame.assetBufferUploadRequests.ExpandOne();
			request.srcBuffer = alloc.buffer;
			request.srcOffset = alloc.offset;
			request.dstBuffer = resources.meshBuffer;
			request.dstOffset = sizeof(ShaderMesh) * handle.h.index;
			request.size = sizeof(ShaderMesh);
		}

		return handle;
	}

	bool RendererAPI::RequestMeshLODBufferAllocation(MeshHandle handle, uint lodIndex, GpuBufferAllocation& allocation)
	{
		RenderResources& resources = m_Renderer.GetRenderResources();
		const Mesh& mesh = m_Registry.GetMesh(handle);

		allocation.buffer = resources.meshLODBuffer;
		allocation.offset = (mesh.lodOffset + lodIndex) * sizeof(ShaderMeshLOD);
		allocation.size = sizeof(ShaderMeshLOD);

		return true;
	}

	bool RendererAPI::RequestVertexBufferAllocation(MeshHandle handle, uint lodIndex, size_t size, GpuBufferAllocation& allocation)
	{
		RenderResources& resources = m_Renderer.GetRenderResources();
		const Mesh& mesh = m_Registry.GetMesh(handle);

		BufferAllocation alloc;
		if (!m_AllocManager.RequestVertexBufferAllocation(size, alloc))
		{
			return false;
		}

		allocation.buffer = resources.vertexBuffer;
		allocation.offset = alloc.offset;
		allocation.size = alloc.size;

		const uint lodOffset = mesh.lodOffset + lodIndex;
		m_AllocManager.GetMeshLODAllocInfo(lodOffset).vertexBufferAllocation = alloc;

		return true;
	}

	bool RendererAPI::RequestIndexBufferAllocation(MeshHandle handle, uint lodIndex, size_t size, GpuBufferAllocation& allocation)
	{
		RenderResources& resources = m_Renderer.GetRenderResources();
		const Mesh& mesh = m_Registry.GetMesh(handle);

		BufferAllocation alloc;
		if (!m_AllocManager.RequestIndexBufferAllocation(size, alloc))
		{
			return false;
		}

		allocation.buffer = resources.indexBuffer;
		allocation.offset = alloc.offset;
		allocation.size = alloc.size;

		const uint lodOffset = mesh.lodOffset + lodIndex;
		m_AllocManager.GetMeshLODAllocInfo(lodOffset).indexBufferAllocation = alloc;

		return true;
	}

	bool RendererAPI::RequestMeshletBufferAllocation(MeshHandle handle, uint lodIndex, size_t size, GpuBufferAllocation& allocation)
	{
		RenderResources& resources = m_Renderer.GetRenderResources();
		const Mesh& mesh = m_Registry.GetMesh(handle);

		BufferAllocation alloc;
		if (!m_AllocManager.RequestMeshletBufferAllocation(size, alloc))
		{
			return false;
		}

		allocation.buffer = resources.meshletBuffer;
		allocation.offset = alloc.offset;
		allocation.size = alloc.size;

		const uint lodOffset = mesh.lodOffset + lodIndex;
		m_AllocManager.GetMeshLODAllocInfo(lodOffset).meshletBufferAllocation = alloc;

		return true;
	}

	// Frees vertex, index and meshlet allocations referenced by the mesh lod
	void RendererAPI::FreeMeshLODAllocations(MeshHandle handle, uint lodIndex)
	{

	}

	void RendererAPI::DeleteMesh(MeshHandle handle)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		renderFrame.meshesToDelete.Add(handle);
	}

	void RendererAPI::RequestBLASBuild(MeshHandle handle)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		renderFrame.meshesToBuildBLAS.Add(handle);
	}

	MeshInstanceHandle RendererAPI::CreateMeshInstance(const MeshInstanceDesc& desc)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		RenderResources& resources = m_Renderer.GetRenderResources();
		const MeshInstanceHandle handle = m_Registry.CreateMeshInstance(desc);
		UploadMeshInstance(desc.info, handle.h.index);
		renderFrame.sceneFrame.meshInstancesToAdd.Add(handle);
		return handle;
	}

	void RendererAPI::UpdateMeshInstance(MeshInstanceHandle handle, const MeshInstanceDesc& desc)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		UploadMeshInstance(desc.info, handle.h.index);
		renderFrame.sceneFrame.meshInstancesToUpdate.Add({ handle, desc });
	}

	void RendererAPI::DeleteMeshInstance(MeshInstanceHandle handle)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		renderFrame.sceneFrame.meshInstancesToRemove.Add(handle);
		renderFrame.meshInstancesToDelete.Add(handle);
	}

	void RendererAPI::UploadMeshInstance(const MeshInstanceInfo& info, uint index)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		RenderResources& resources = m_Renderer.GetRenderResources();

		ShaderMeshInstance shaderMeshInstance;
		shaderMeshInstance.transform = info.transform;
		shaderMeshInstance.meshIndex = info.mesh.h.index;
		for (uint i = 0; i < MeshConstants::c_MaxSubmeshes; ++i)
		{
			shaderMeshInstance.materialIndices[i] = i < info.materials.Size() ? info.materials[i].h.index : c_InvalidRenderIndex;
		}

		UploadBufferAllocation alloc;
		const bool uploadSuccess = m_AllocManager.RequestFrameUploadAllocation(sizeof(ShaderMeshInstance), alloc);
		TYR_ASSERT(uploadSuccess);

		if (uploadSuccess)
		{
			RenderResourceUtil::WriteUploadBuffer(m_Registry.GetBuffer(alloc.buffer), m_Device, alloc.offset, &shaderMeshInstance, sizeof(ShaderMeshInstance));

			BufferUploadRequest& request = renderFrame.frameBufferUploadRequests.ExpandOne();
			request.srcBuffer = alloc.buffer;
			request.srcOffset = alloc.offset;
			request.dstBuffer = resources.meshInstanceBuffer;
			request.dstOffset = sizeof(ShaderMeshInstance) * index;
			request.size = sizeof(ShaderMeshInstance);
		}
	}

	void RendererAPI::SetSceneWindow(SceneHandle scene, RenderWindowHandle window)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		renderFrame.sceneFrame.newWindow = window;
		m_Renderer.GetImmediateSceneData(scene).windowHandle = window;
	}

	void RendererAPI::AddView(const SceneView& view)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		renderFrame.sceneFrame.views.Add(view);
	}

	void RendererAPI::SetSceneAmbient(SceneHandle scene, float ambient)
	{
		m_Renderer.GetImmediateSceneData(scene).ambient = ambient;
	}

	DirLightHandle RendererAPI::CreateDirectionalLight(SceneHandle scene, const DirectionalLightDesc& desc)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		const DirLightHandle handle = m_Registry.CreateDirectionalLight(desc);
		UploadDirectionalLight(desc.info, handle.h.index);
		renderFrame.sceneFrame.dirLightsToAdd.Add(handle);
		m_Renderer.GetImmediateSceneData(scene).dirLightCount++;
		return handle;
	}

	void RendererAPI::UpdateDirectionalLight(DirLightHandle handle, const DirectionalLightDesc& desc)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		UploadDirectionalLight(desc.info, handle.h.index);
		renderFrame.sceneFrame.dirLightsToUpdate.Add({ handle, desc });
	}

	void RendererAPI::DeleteDirectionalLight(SceneHandle scene, DirLightHandle handle)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		renderFrame.sceneFrame.dirLightsToRemove.Add(handle);
		renderFrame.dirLightsToDelete.Add(handle);
		m_Renderer.GetImmediateSceneData(scene).dirLightCount--;
	}

	PointLightHandle RendererAPI::CreatePointLight(SceneHandle scene, const PointLightDesc& desc)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		const PointLightHandle handle = m_Registry.CreatePointLight(desc);
		UploadPointLight(desc.info, handle.h.index);
		renderFrame.sceneFrame.pointLightsToAdd.Add(handle);
		m_Renderer.GetImmediateSceneData(scene).pointLightCount++;
		return handle;
	}

	void RendererAPI::UpdatePointLight(PointLightHandle handle, const PointLightDesc& desc)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		UploadPointLight(desc.info, handle.h.index);
		renderFrame.sceneFrame.pointLightsToUpdate.Add({ handle, desc });
	}

	void RendererAPI::DeletePointLight(SceneHandle scene, PointLightHandle handle)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		renderFrame.sceneFrame.pointLightsToRemove.Add(handle);
		renderFrame.pointLightsToDelete.Add(handle);
		m_Renderer.GetImmediateSceneData(scene).pointLightCount--;
	}

	SpotLightHandle RendererAPI::CreateSpotLight(SceneHandle scene, const SpotLightDesc& desc)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		const SpotLightHandle handle = m_Registry.CreateSpotLight(desc);
		UploadSpotLight(desc.info, handle.h.index);
		renderFrame.sceneFrame.spotLightsToAdd.Add(handle);
		m_Renderer.GetImmediateSceneData(scene).spotLightCount++;
		return handle;
	}

	void RendererAPI::UpdateSpotLight(SpotLightHandle handle, const SpotLightDesc& desc)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		UploadSpotLight(desc.info, handle.h.index);
		renderFrame.sceneFrame.spotLightsToUpdate.Add({ handle, desc });
	}

	void RendererAPI::DeleteSpotLight(SceneHandle scene, SpotLightHandle handle)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		renderFrame.sceneFrame.spotLightsToRemove.Add(handle);
		renderFrame.spotLightsToDelete.Add(handle);
		m_Renderer.GetImmediateSceneData(scene).spotLightCount--;
	}

	void RendererAPI::UploadDirectionalLight(const DirectionalLightInfo& info, uint index)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		RenderResources& resources = m_Renderer.GetRenderResources();

		ShaderDirectionalLight shaderLight{};
		shaderLight.direction = info.direction;
		shaderLight.intensity = info.intensity;
		shaderLight.colour = info.colour;

		UploadBufferAllocation alloc;
		const bool uploadSuccess = m_AllocManager.RequestFrameUploadAllocation(sizeof(ShaderDirectionalLight), alloc);
		TYR_ASSERT(uploadSuccess);

		if (uploadSuccess)
		{
			RenderResourceUtil::WriteUploadBuffer(m_Registry.GetBuffer(alloc.buffer), m_Device, alloc.offset, &shaderLight, sizeof(ShaderDirectionalLight));

			BufferUploadRequest& request = renderFrame.frameBufferUploadRequests.ExpandOne();
			request.srcBuffer = alloc.buffer;
			request.srcOffset = alloc.offset;
			request.dstBuffer = resources.directionalLightBuffer;
			request.dstOffset = sizeof(ShaderDirectionalLight) * index;
			request.size = sizeof(ShaderDirectionalLight);
		}
	}

	void RendererAPI::UploadPointLight(const PointLightInfo& info, uint index)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		RenderResources& resources = m_Renderer.GetRenderResources();

		ShaderPointLight shaderLight{};
		shaderLight.position = info.position;
		shaderLight.range = info.range;
		shaderLight.intensity = info.intensity;
		shaderLight.attenuation = info.attenuation;
		shaderLight.colour = info.colour;


		UploadBufferAllocation alloc;
		const bool uploadSuccess = m_AllocManager.RequestFrameUploadAllocation(sizeof(ShaderPointLight), alloc);
		TYR_ASSERT(uploadSuccess);

		if (uploadSuccess)
		{
			RenderResourceUtil::WriteUploadBuffer(m_Registry.GetBuffer(alloc.buffer), m_Device, alloc.offset, &shaderLight, sizeof(ShaderPointLight));

			BufferUploadRequest& request = renderFrame.frameBufferUploadRequests.ExpandOne();
			request.srcBuffer = alloc.buffer;
			request.srcOffset = alloc.offset;
			request.dstBuffer = resources.pointLightBuffer;
			request.dstOffset = sizeof(ShaderPointLight) * index;
			request.size = sizeof(ShaderPointLight);
		}
	}

	void RendererAPI::UploadSpotLight(const SpotLightInfo& info, uint index)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		RenderResources& resources = m_Renderer.GetRenderResources();

		ShaderSpotLight shaderLight{};
		shaderLight.position = info.position;
		shaderLight.range = info.range;
		shaderLight.cone = info.cone;
		shaderLight.direction = info.direction;
		shaderLight.attenuation = info.attenuation;
		shaderLight.intensity = info.intensity;
		shaderLight.colour = info.colour;

		UploadBufferAllocation alloc;
		const bool uploadSuccess = m_AllocManager.RequestFrameUploadAllocation(sizeof(ShaderSpotLight), alloc);
		TYR_ASSERT(uploadSuccess);

		if (uploadSuccess)
		{
			RenderResourceUtil::WriteUploadBuffer(m_Registry.GetBuffer(alloc.buffer), m_Device, alloc.offset, &shaderLight, sizeof(ShaderSpotLight));

			BufferUploadRequest& request = renderFrame.frameBufferUploadRequests.ExpandOne();
			request.srcBuffer = alloc.buffer;
			request.srcOffset = alloc.offset;
			request.dstBuffer = resources.spotLightBuffer;
			request.dstOffset = sizeof(ShaderSpotLight) * index;
			request.size = sizeof(ShaderSpotLight);
		}
	}

	bool RendererAPI::RequestResourceUploadAllocation(size_t size, UploadBufferAllocation& allocation)
	{
		return m_AllocManager.RequestResourceUploadAllocation(size, allocation);
	}

	void RendererAPI::FlushBufferUploadAllocation(const UploadBufferAllocation& alloc)
	{
		RenderBuffer& buffer = m_Registry.GetBuffer(alloc.buffer);
		m_Device.FlushBufferAllocation(buffer.buffer, alloc.offset, alloc.size);
	}

	void RendererAPI::SubmitGUIDrawData(const GUIDrawData& data)
	{
		if (data.vertices.Size() == 0 || data.indices.Size() == 0)
		{
			return;
		}

#if TYR_RENDER_DEBUG
		// TEMP DEBUG - flicker investigation. Checks the final CPU-side vertex array - common to
		// both the ImGui and Nuklear submission paths - immediately before anything in the
		// upload/GPU pipeline touches it. If this ever fires, the NaN is already present on the
		// CPU before WriteUploadBuffer/FrameUploadAllocator/TransferPass get involved at all,
		// ruling all of that out in one shot. Revert after.
		for (uint v = 0; v < data.vertices.Size(); ++v)
		{
			const GUIVertex& vertex = data.vertices[v];
			TYR_ASSERT(vertex.pos.x == vertex.pos.x && vertex.pos.y == vertex.pos.y
				&& vertex.uv.x == vertex.uv.x && vertex.uv.y == vertex.uv.y);
		}
#endif

		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		RenderResources& resources = m_Renderer.GetRenderResources();
		// guiVertexBuffer/guiIndexBuffer are each one physical buffer, c_BufferedFrameCount slots
		// big - this slot's own writes/reads are confined to its own c_BufferedFrameCount-th of it
		// (see RenderConstants::c_GUIVertexBufferSize's own comment on why), at this fixed byte
		// base. GUIPass::Execute applies the matching base on the read side (BindIndexBuffer's
		// offset, DrawIndexed's vertexOffset).
		const uint renderFrameIndex = m_Renderer.GetRenderFrameIndex();
		const size_t vertexBase = renderFrameIndex * RenderConstants::c_GUIVertexBufferSize;
		const size_t indexBase = renderFrameIndex * RenderConstants::c_GUIIndexBufferSize;

		const size_t vertexBytes = sizeof(GUIVertex) * data.vertices.Size();
		const size_t indexBytes = sizeof(uint16) * data.indices.Size();

		TYR_ASSERT((renderFrame.guiVertexCursor + data.vertices.Size()) * sizeof(GUIVertex) <= RenderConstants::c_GUIVertexBufferSize);
		TYR_ASSERT((renderFrame.guiIndexCursor + data.indices.Size()) * sizeof(uint16) <= RenderConstants::c_GUIIndexBufferSize);

		UploadBufferAllocation vertexAlloc;
		const bool vertexUploadOk = m_AllocManager.RequestFrameUploadAllocation(vertexBytes, vertexAlloc);
		TYR_ASSERT(vertexUploadOk);
		if (vertexUploadOk)
		{
			RenderResourceUtil::WriteUploadBuffer(m_Registry.GetBuffer(vertexAlloc.buffer), m_Device, vertexAlloc.offset, (void*)data.vertices.Data(), vertexBytes);

			BufferUploadRequest& request = renderFrame.frameBufferUploadRequests.ExpandOne();
			request.srcBuffer = vertexAlloc.buffer;
			request.srcOffset = vertexAlloc.offset;
			request.dstBuffer = resources.guiVertexBuffer;
			request.dstOffset = vertexBase + renderFrame.guiVertexCursor * sizeof(GUIVertex);
			request.size = vertexBytes;
		}

		UploadBufferAllocation indexAlloc;
		const bool indexUploadOk = m_AllocManager.RequestFrameUploadAllocation(indexBytes, indexAlloc);
		TYR_ASSERT(indexUploadOk);
		if (indexUploadOk)
		{
			RenderResourceUtil::WriteUploadBuffer(m_Registry.GetBuffer(indexAlloc.buffer), m_Device, indexAlloc.offset, (void*)data.indices.Data(), indexBytes);

			BufferUploadRequest& request = renderFrame.frameBufferUploadRequests.ExpandOne();
			request.srcBuffer = indexAlloc.buffer;
			request.srcOffset = indexAlloc.offset;
			request.dstBuffer = resources.guiIndexBuffer;
			request.dstOffset = indexBase + renderFrame.guiIndexCursor * sizeof(uint16);
			request.size = indexBytes;
		}

		GUIDrawSubmission& submission = renderFrame.guiDrawData.ExpandOne();
		submission.displaySize = data.displaySize;
		submission.commands = data.commands;
		submission.vertexOffset = renderFrame.guiVertexCursor;
		submission.indexOffset = renderFrame.guiIndexCursor;

		renderFrame.guiVertexCursor += (uint)data.vertices.Size();
		renderFrame.guiIndexCursor += (uint)data.indices.Size();
	}

	void RendererAPI::ResetGUIDrawData()
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		renderFrame.guiDrawData.Clear();
		renderFrame.guiVertexCursor = 0;
		renderFrame.guiIndexCursor = 0;
	}
}