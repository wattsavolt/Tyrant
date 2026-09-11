#include "RendererAPI.h"
#include "Renderer.h"
#include "RenderResource/RenderResourceUtil.h"
#include "RenderAPI/Device.h"
#include "RenderRegistry.h"
#include "RenderTransfer/RenderTransferTypes.h"

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

	void RendererAPI::ResizeWindow(RenderWindowHandle window, uint width, uint height)
	{
		m_Renderer.ResizeWindow(window, width, height);
	}

	uint RendererAPI::AddScene(const char* name)
	{
		// It's okay to do these modifications to the render data on this thread as any async thread doing rendering work
		// would only be modifying the active scene which won't be touched here
		RenderData& data = m_Renderer.GetRenderData();
		static uint nextID = 0;
		if (data.sceneCount < RenderConstants::c_MaxScenes)
		{
			for (uint8 i = 0; i < RenderConstants::c_MaxScenes; ++i)
			{
				if (!data.scenes[i].inUse)
				{
					data.scenes[i].name = name;
					data.scenes[i].id = nextID++;
					data.scenes[i].inUse = true;
					return i;
				}
			}
		}

		TYR_ASSERT(false);
		return UINT32_MAX;
	}

	void RendererAPI::RemoveScene(uint index)
	{
		// The caller must ensure the scene is finished with by the renderer on the CPU and GPU
		RenderData& data = m_Renderer.GetRenderData();
		if (data.sceneCount == 0 || index >= RenderConstants::c_MaxScenes || !data.scenes[index].inUse)
		{
			TYR_ASSERT(false);
			return;
		}

		data.scenes[index].inUse = false;
		data.scenes[index].Clear();
	}

	void RendererAPI::SetActiveSceneIndex(uint index, bool visible)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		renderFrame.activeSceneIndex = index;
		renderFrame.sceneFrame.visible = visible;
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
			request.dstBuffer = resources.spotLightBuffer;
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
		shaderMeshInstance.materialIndex = info.material.h.index;

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

	void RendererAPI::SetSceneWindow(RenderWindowHandle window)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		renderFrame.sceneFrame.newWindow = window;
	}

	void RendererAPI::AddView(const SceneView& view)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		renderFrame.sceneFrame.views.Add(view);
	}

	DirLightHandle RendererAPI::CreateDirectionalLight(const DirectionalLightDesc& desc)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		const DirLightHandle handle = m_Registry.CreateDirectionalLight(desc);
		UploadDirectionalLight(desc.info, handle.h.index);
		renderFrame.sceneFrame.dirLightsToAdd.Add(handle);
		return handle;
	}

	void RendererAPI::UpdateDirectionalLight(DirLightHandle handle, const DirectionalLightDesc& desc)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		UploadDirectionalLight(desc.info, handle.h.index);
		renderFrame.sceneFrame.dirLightsToUpdate.Add({ handle, desc });
	}

	void RendererAPI::DeleteDirectionalLight(DirLightHandle handle)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		renderFrame.sceneFrame.dirLightsToRemove.Add(handle);
		renderFrame.dirLightsToDelete.Add(handle);
	}

	PointLightHandle RendererAPI::CreatePointLight(const PointLightDesc& desc)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		const PointLightHandle handle = m_Registry.CreatePointLight(desc);
		UploadPointLight(desc.info, handle.h.index);
		renderFrame.sceneFrame.pointLightsToAdd.Add(handle);
		return handle;
	}

	void RendererAPI::UpdatePointLight(PointLightHandle handle, const PointLightDesc& desc)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		UploadPointLight(desc.info, handle.h.index);
		renderFrame.sceneFrame.pointLightsToUpdate.Add({ handle, desc });
	}

	void RendererAPI::DeletePointLight(PointLightHandle handle)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		renderFrame.sceneFrame.pointLightsToRemove.Add(handle);
		renderFrame.pointLightsToDelete.Add(handle);
	}

	SpotLightHandle RendererAPI::CreateSpotLight(const SpotLightDesc& desc)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		const SpotLightHandle handle = m_Registry.CreateSpotLight(desc);
		UploadSpotLight(desc.info, handle.h.index);
		renderFrame.sceneFrame.spotLightsToAdd.Add(handle);
		return handle;
	}

	void RendererAPI::UpdateSpotLight(SpotLightHandle handle, const SpotLightDesc& desc)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		UploadSpotLight(desc.info, handle.h.index);
		renderFrame.sceneFrame.spotLightsToUpdate.Add({ handle, desc });
	}

	void RendererAPI::DeleteSpotLight(SpotLightHandle handle)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		renderFrame.sceneFrame.spotLightsToRemove.Add(handle);
		renderFrame.spotLightsToDelete.Add(handle);
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
}