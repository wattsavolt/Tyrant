#include "RendererAPI.h"
#include "RenderDebug.h"
#include "Renderer.h"
#include "RenderResource/RenderResourceUtil.h"
#include "RenderAPI/Device.h"
#include "RenderRegistry.h"
#include "RenderTransfer/RenderTransferTypes.h"
#include "GUIDrawData.h"
#include "RenderResource/Mesh.h"
#include "RenderTransfer/BufferAllocation.h"
#include "Geometry/BoundingSphere.h"
#include <algorithm>

namespace tyr
{
	namespace
	{
		template<typename LightHandleType, uint N>
		void AddSceneLight(LocalArray<LightHandleType, N>& lights, LightHandleType handle)
		{
			TYR_ASSERT(lights.Size() < N);
			lights.Add(handle);
		}

		// Returns false if the handle wasn't in the list.
		template<typename HandleType>
		bool RemoveFromList(Array<HandleType>& list, HandleType handle)
		{
			for (uint i = 0; i < list.Size(); ++i)
			{
				if (list[i] == handle)
				{
					list.SwapAndPopBack(i);
					return true;
				}
			}
			return false;
		}

		template<typename LightHandleType, uint N>
		void RemoveSceneLight(LocalArray<LightHandleType, N>& lights, LightHandleType handle)
		{
			for (uint i = 0; i < lights.Size(); ++i)
			{
				if (lights[i] == handle)
				{
					lights.SwapAndPopBack(i);
					return;
				}
			}
		}
	}

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
		// Covers pool-slot reuse.
		m_Renderer.GetImmediateSceneData(handle).Reset();
		return handle;
	}

	void RendererAPI::RemoveScene(SceneHandle handle)
	{
		// The scene's data can't be reset right here - RenderAsync might still be reading/writing
		// it from frames ago. Clearing it as the active scene means no new task will reference
		// it, so every existing reference is guaranteed done once this cycles back around.
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
		// Must match whatever layout this texture is actually kept in - a mismatch here is
		// invalid descriptor usage and reads back as garbage/black regardless of how correct the
		// underlying image data is.
		imageInfo.layout = texture.imageLayout;

		// Queued rather than written immediately.
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

		// Only ever (re)creates this tick's own slot's targets - the other slots keep whatever
		// they already have until their own turn comes back around. Main thread only, like every
		// RendererAPI Create*/Delete* call.
		const uint renderFrameIndex = m_Renderer.GetRenderFrameIndex();
		const RenderViewportTextureData& current = rv.textureData[renderFrameIndex];
		if (!current.colourTexture || current.width != width || current.height != height)
		{
			m_Renderer.ResizeRenderViewportSlot(rv, renderFrameIndex, name, width, height);
		}

		// Record the latest requested size and flag the other two buffered slots to pick it up on
		// their own next turn.
		rv.requestedWidth = width;
		rv.requestedHeight = height;
		for (uint i = 1; i < RenderConstants::c_BufferedFrameCount; ++i)
		{
			const uint otherSlot = (renderFrameIndex + i) % RenderConstants::c_BufferedFrameCount;
			rv.pendingResize[otherSlot] = true;
		}

		// Which one actually holds this tick's "real" result - read via the main-thread-safe
		// mirror, not Renderer::m_TaaEnabled directly, since that's worker-owned (see its own
		// comment). The two can briefly disagree for a tick or two right after toggling TAA (the
		// override takes a moment to reach RenderAsync) - a harmless, self-correcting display lag,
		// not a hazard, since both textures are always kept in a valid state by their own passes
		// regardless of which one is actually selected here.
		const RenderViewportTextureData& textureData = rv.textureData[renderFrameIndex];
		if (rv.grid.enabled && textureData.overlayColourTexture)
		{
			return textureData.overlayColourTexture;
		}
		return m_Renderer.IsTaaEnabledMainThread() ? textureData.resolvedColourTexture : textureData.colourTexture;
	}

	void RendererAPI::SetRenderViewportGrid(RenderViewportHandle viewport, const ViewportGridDesc& desc)
	{
		// The overlay textures follow on each buffered slot's own next turn.
		m_Renderer.GetRenderViewportPool()[viewport.h].grid = desc;
	}

	MaterialHandle RendererAPI::CreateMaterial(const MaterialDesc& desc)
	{
		const MaterialHandle handle = m_Registry.CreateMaterial(desc);
		UploadMaterial(handle, desc);
		return handle;
	}

	void RendererAPI::UpdateMaterial(MaterialHandle handle, const MaterialDesc& desc)
	{
		m_Registry.UpdateMaterial(handle, desc);
		UploadMaterial(handle, desc);
	}

	void RendererAPI::UploadMaterial(MaterialHandle handle, const MaterialDesc& desc)
	{
		TYR_STATIC_ASSERT(sizeof(ShaderMaterial) == 80, "ShaderMaterial must match the shaders' 16 byte rows.");
		TYR_STATIC_ASSERT(static_cast<uint>(MaterialBlendMode::Masked) == TYR_MATERIAL_BLEND_MODE_MASKED, "Keep in sync with ShaderTypes.h.");
		TYR_ASSERT(desc.type == MaterialType::PBR);
		const MaterialParams& params = desc.params;

		ShaderMaterial shaderMaterial;
		shaderMaterial.texture0 = desc.textures[MaterialConstants::c_PbrAlbedoIndex].index;
		shaderMaterial.texture1 = desc.textures[MaterialConstants::c_PbrNormalHeightIndex].index;
		shaderMaterial.texture2 = desc.textures[MaterialConstants::c_PbrAoRoughnessMetallicIndex].index;
		shaderMaterial.flags = params.blendMode;
		shaderMaterial.albedoTint = params.albedoTint;
		shaderMaterial.uvScale = params.uvScale;
		shaderMaterial.uvOffset = params.uvOffset;
		shaderMaterial.roughnessRange = Vector2(params.roughnessMin, params.roughnessMax);
		shaderMaterial.metallicRange = Vector2(params.metallicMin, params.metallicMax);
		shaderMaterial.normalStrength = params.normalStrength;
		shaderMaterial.occlusionStrength = params.occlusionStrength;
		shaderMaterial.heightScale = params.heightScale;
		shaderMaterial.alphaCutoff = params.alphaCutoff;

		m_Renderer.GetRecords().materials.Set(handle.h.index, shaderMaterial);
		m_Renderer.GetRenderFrame().materialBlendModeUpdates.Add({ handle.h.index, params.blendMode });
	}

	void RendererAPI::DeleteMaterial(MaterialHandle handle)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		m_Renderer.GetRecords().materials.ClearDirty(handle.h.index);
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

	bool RendererAPI::RequestSkinVertexBufferAllocation(MeshHandle handle, uint lodIndex, size_t size, GpuBufferAllocation& allocation)
	{
		RenderResources& resources = m_Renderer.GetRenderResources();
		const Mesh& mesh = m_Registry.GetMesh(handle);

		BufferAllocation alloc;
		if (!m_AllocManager.RequestSkinVertexBufferAllocation(size, alloc))
		{
			return false;
		}

		allocation.buffer = resources.skinVertexBuffer;
		allocation.offset = alloc.offset;
		allocation.size = alloc.size;

		m_AllocManager.GetMeshLODAllocInfo(mesh.lodOffset + lodIndex).skinBufferAllocation = alloc;
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

	MeshInstanceHandle RendererAPI::CreateMeshInstance(SceneHandle scene, const MeshInstanceDesc& desc)
	{
		TYR_ASSERT(scene);
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		const MeshInstanceHandle handle = m_Registry.CreateMeshInstance(desc);
		m_Registry.GetMeshInstance(handle).scene = scene;
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
		m_Renderer.GetRecords().meshInstances.ClearDirty(handle.h.index);
		// One created this frame hasn't reached its scene yet, so it's only taken back out.
		if (!RemoveFromList(renderFrame.sceneFrame.meshInstancesToAdd, handle))
		{
			renderFrame.sceneFrame.meshInstancesToRemove.Add(handle);
		}
		renderFrame.meshInstancesToDelete.Add(handle);
	}

	SkeletalMeshInstanceHandle RendererAPI::CreateSkeletalMeshInstance(SceneHandle scene, const SkeletalMeshInstanceDesc& desc)
	{
		TYR_ASSERT(scene);
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		const Mesh& mesh = m_Registry.GetMesh(desc.info.mesh);
		TYR_ASSERT(mesh.skinned);

		// Only LOD0 is skinned. This frame's vertices are followed by last frame's.
		const MeshLODAllocInfo& lodAlloc = m_AllocManager.GetMeshLODAllocInfo(mesh.lodOffset);
		const uint vertexCount = (uint)(lodAlloc.vertexBufferAllocation.size / sizeof(ShaderVertex));
		BufferAllocation skinnedAlloc;
		if (!m_AllocManager.RequestVertexBufferAllocation(sizeof(ShaderVertex) * vertexCount * 2, skinnedAlloc))
		{
			TYR_LOG_ERROR("Out of vertex buffer space for a skinned mesh instance.");
			return {};
		}

		MeshInstanceDesc meshInstanceDesc;
		meshInstanceDesc.info.transform = desc.info.transform;
		meshInstanceDesc.info.mesh = desc.info.mesh;
		meshInstanceDesc.info.materials = desc.info.materials;
		const MeshInstanceHandle meshInstance = m_Registry.CreateMeshInstance(meshInstanceDesc);
		m_Registry.GetMeshInstance(meshInstance).scene = scene;

		const SkeletalMeshInstanceHandle handle = m_Registry.CreateSkeletalMeshInstance(desc);
		SkeletalMeshInstance& instance = m_Registry.GetSkeletalMeshInstance(handle);
		instance.skinnedSphere = mesh.sphere;
		instance.scene = scene;
		instance.meshInstance = meshInstance;
		instance.mesh = desc.info.mesh;
		instance.skinnedVertexAllocation = skinnedAlloc;
		instance.restVertexOffset = (uint)(lodAlloc.vertexBufferAllocation.offset / sizeof(ShaderVertex));
		instance.skinVertexOffset = (uint)(lodAlloc.skinBufferAllocation.offset / sizeof(ShaderSkinVertex));
		instance.outputVertexOffset = (uint)(skinnedAlloc.offset / sizeof(ShaderVertex));
		instance.prevOutputVertexOffset = instance.outputVertexOffset + vertexCount;
		instance.vertexCount = vertexCount;

		// Identity bone matrices leave it in the bind pose until a pose is set.
		Matrix4 bindPose[TYR_MAX_BONES];
		for (Matrix4& matrix : bindPose)
		{
			matrix = Matrix4::c_Identity;
		}
		UpdateSkeletalMeshPose(handle, bindPose, TYR_MAX_BONES, mesh.sphere);

		renderFrame.sceneFrame.meshInstancesToAdd.Add(meshInstance);
		renderFrame.sceneFrame.skeletalMeshInstancesToAdd.Add(handle);
		m_Renderer.GetSkinnedInstances().Add(handle);
		return handle;
	}

	void RendererAPI::UpdateSkeletalMeshInstance(SkeletalMeshInstanceHandle handle, const SkeletalMeshInstanceDesc& desc)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		SkeletalMeshInstance& instance = m_Registry.GetSkeletalMeshInstance(handle);
		TYR_ASSERT(desc.info.mesh == instance.mesh);
		instance.info = desc.info;
		UploadSkinnedMeshInstance(instance);

		// Ray tracing reads the transform from the mesh instance, on the worker thread.
		MeshInstanceUpdate& update = renderFrame.sceneFrame.meshInstancesToUpdate.ExpandOne();
		update.handle = instance.meshInstance;
		update.desc.info.transform = desc.info.transform;
		update.desc.info.mesh = desc.info.mesh;
		update.desc.info.materials = desc.info.materials;
	}

	void RendererAPI::UpdateSkeletalMeshPose(SkeletalMeshInstanceHandle handle, const Matrix4* skinMatrices, uint boneCount, const BoundingSphere& localBounds)
	{
		TYR_ASSERT(boneCount <= TYR_MAX_BONES);
		SkeletalMeshInstance& instance = m_Registry.GetSkeletalMeshInstance(handle);

		instance.skinnedSphere = localBounds;
		UploadSkinnedMeshInstance(instance);

		BonePalette& palette = m_Renderer.GetRecords().bonePalettes.GetRecord(handle.h.index);
		memcpy(palette.matrices, skinMatrices, sizeof(Matrix4) * boneCount);
		palette.boneCount = boneCount;
		m_Renderer.GetRecords().bonePalettes.Set(handle.h.index, palette);
	}

	void RendererAPI::DeleteSkeletalMeshInstance(SkeletalMeshInstanceHandle handle)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		SceneFrame& sceneFrame = renderFrame.sceneFrame;
		const SkeletalMeshInstance& instance = m_Registry.GetSkeletalMeshInstance(handle);

		// One created this frame hasn't reached the scene yet, so it's only taken back out.
		if (!RemoveFromList(sceneFrame.meshInstancesToAdd, instance.meshInstance))
		{
			sceneFrame.meshInstancesToRemove.Add(instance.meshInstance);
		}
		if (!RemoveFromList(sceneFrame.skeletalMeshInstancesToAdd, handle))
		{
			sceneFrame.skeletalMeshInstancesToRemove.Add(handle);
		}
		renderFrame.meshInstancesToDelete.Add(instance.meshInstance);
		renderFrame.skeletalMeshInstancesToDelete.Add(handle);
		m_Renderer.GetRecords().meshInstances.ClearDirty(instance.meshInstance.h.index);
		m_Renderer.GetRecords().bonePalettes.ClearDirty(handle.h.index);

		RemoveFromList(m_Renderer.GetSkinnedInstances(), handle);
	}

	void RendererAPI::UploadSkinnedMeshInstance(const SkeletalMeshInstance& instance)
	{
		ShaderMeshInstance shaderMeshInstance;
		shaderMeshInstance.transform = instance.info.transform;
		shaderMeshInstance.meshIndex = instance.mesh.h.index;
		for (uint i = 0; i < MeshConstants::c_MaxSubmeshes; ++i)
		{
			shaderMeshInstance.materialIndices[i] = i < instance.info.materials.Size() ? instance.info.materials[i].h.index : c_InvalidRenderIndex;
		}
		shaderMeshInstance.skinnedVertexOffset = instance.outputVertexOffset;
		shaderMeshInstance.prevSkinnedVertexOffset = instance.prevOutputVertexOffset;
		const BoundingSphere& sphere = instance.skinnedSphere;
		shaderMeshInstance.skinnedSphere = Vector4(sphere.m_Centre.x, sphere.m_Centre.y, sphere.m_Centre.z, sphere.m_Radius);
		WriteMeshInstance(shaderMeshInstance, instance.meshInstance.h.index);
	}

	void RendererAPI::UploadMeshInstance(const MeshInstanceInfo& info, uint index)
	{
		ShaderMeshInstance shaderMeshInstance;
		shaderMeshInstance.transform = info.transform;
		shaderMeshInstance.meshIndex = info.mesh.h.index;
		for (uint i = 0; i < MeshConstants::c_MaxSubmeshes; ++i)
		{
			shaderMeshInstance.materialIndices[i] = i < info.materials.Size() ? info.materials[i].h.index : c_InvalidRenderIndex;
		}
		shaderMeshInstance.skinnedVertexOffset = TYR_INVALID_INDEX;
		shaderMeshInstance.prevSkinnedVertexOffset = TYR_INVALID_INDEX;
		shaderMeshInstance.skinnedSphere = Vector4(0.0f, 0.0f, 0.0f, 0.0f);
		WriteMeshInstance(shaderMeshInstance, index);
	}

	void RendererAPI::WriteMeshInstance(const ShaderMeshInstance& shaderMeshInstance, uint index)
	{
		m_Renderer.GetRecords().meshInstances.Set(index, shaderMeshInstance);
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
		const DirLightHandle handle = m_Registry.CreateDirectionalLight(desc);
		UploadDirectionalLight(desc.info, handle.h.index);
		AddSceneLight(m_Renderer.GetImmediateSceneData(scene).lights.dirLights, handle);
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
		RemoveSceneLight(m_Renderer.GetImmediateSceneData(scene).lights.dirLights, handle);
		renderFrame.dirLightsToDelete.Add(handle);
		m_Renderer.GetRecords().dirLights.ClearDirty(handle.h.index);
	}

	PointLightHandle RendererAPI::CreatePointLight(SceneHandle scene, const PointLightDesc& desc)
	{
		const PointLightHandle handle = m_Registry.CreatePointLight(desc);
		UploadPointLight(desc.info, handle.h.index);
		AddSceneLight(m_Renderer.GetImmediateSceneData(scene).lights.pointLights, handle);
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
		RemoveSceneLight(m_Renderer.GetImmediateSceneData(scene).lights.pointLights, handle);
		renderFrame.pointLightsToDelete.Add(handle);
		m_Renderer.GetRecords().pointLights.ClearDirty(handle.h.index);
	}

	SpotLightHandle RendererAPI::CreateSpotLight(SceneHandle scene, const SpotLightDesc& desc)
	{
		const SpotLightHandle handle = m_Registry.CreateSpotLight(desc);
		UploadSpotLight(desc.info, handle.h.index);
		AddSceneLight(m_Renderer.GetImmediateSceneData(scene).lights.spotLights, handle);
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
		RemoveSceneLight(m_Renderer.GetImmediateSceneData(scene).lights.spotLights, handle);
		renderFrame.spotLightsToDelete.Add(handle);
		m_Renderer.GetRecords().spotLights.ClearDirty(handle.h.index);
	}

	void RendererAPI::UploadDirectionalLight(const DirectionalLightInfo& info, uint index)
	{
		ShaderDirectionalLight shaderLight{};
		shaderLight.direction = info.direction;
		shaderLight.intensity = info.intensity;
		shaderLight.colour = info.colour;

		m_Renderer.GetRecords().dirLights.Set(index, shaderLight);
	}

	void RendererAPI::UploadPointLight(const PointLightInfo& info, uint index)
	{
		ShaderPointLight shaderLight{};
		shaderLight.position = info.position;
		shaderLight.range = info.range;
		shaderLight.intensity = info.intensity;
		shaderLight.attenuation = info.attenuation;
		shaderLight.colour = info.colour;


		m_Renderer.GetRecords().pointLights.Set(index, shaderLight);
	}

	void RendererAPI::UploadSpotLight(const SpotLightInfo& info, uint index)
	{
		ShaderSpotLight shaderLight{};
		shaderLight.position = info.position;
		shaderLight.range = info.range;
		shaderLight.cone = info.cone;
		shaderLight.direction = info.direction;
		shaderLight.attenuation = info.attenuation;
		shaderLight.intensity = info.intensity;
		shaderLight.colour = info.colour;

		m_Renderer.GetRecords().spotLights.Set(index, shaderLight);
	}

	DecalHandle RendererAPI::CreateDecal(SceneHandle scene, const DecalDesc& desc)
	{
		LocalArray<DecalHandle, RenderConstants::c_MaxDecals>& decals = m_Renderer.GetImmediateSceneData(scene).decals;
		if (decals.Size() == RenderConstants::c_MaxDecals)
		{
			TYR_LOG_WARNING("A level can't have more than %u decals.", RenderConstants::c_MaxDecals);
			return {};
		}

		const DecalHandle handle = m_Registry.CreateDecal();
		UploadDecal(handle, desc.info);
		decals.Add(handle);
		return handle;
	}

	void RendererAPI::UpdateDecal(DecalHandle handle, const DecalDesc& desc)
	{
		UploadDecal(handle, desc.info);
	}

	void RendererAPI::DeleteDecal(SceneHandle scene, DecalHandle handle)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		RemoveSceneLight(m_Renderer.GetImmediateSceneData(scene).decals, handle);
		renderFrame.decalsToDelete.Add(handle);
		m_Renderer.GetRecords().decals.ClearDirty(handle.h.index);
	}

	void RendererAPI::UploadDecal(DecalHandle handle, const DecalInfo& info)
	{
		const Vector3 size(Math::Abs(info.scale.x), Math::Abs(info.scale.y), Math::Abs(info.scale.z));
		Decal& decal = m_Registry.GetDecal(handle);
		decal.info = info;
		decal.centre = info.position;
		decal.radius = (size * 0.5f).Length();

		// A flat box would have no inverse, so each side keeps a little size.
		constexpr float c_MinSize = 0.001f;
		const Vector3 safeScale(std::max(size.x, c_MinSize), std::max(size.y, c_MinSize), std::max(size.z, c_MinSize));

		ShaderDecal shaderDecal{};
		shaderDecal.worldToBox = Matrix4::CreateInverseTRS(info.position, info.rotation, safeScale);
		shaderDecal.boundsCentre = decal.centre;
		shaderDecal.boundsRadius = decal.radius;
		shaderDecal.tangent = info.rotation.Rotate(Vector3::c_Right);
		shaderDecal.normal = info.rotation.Rotate(Vector3::c_Up);
		shaderDecal.invWidth = 1.0f / safeScale.x;
		shaderDecal.invLength = 1.0f / safeScale.z;
		shaderDecal.materialIndex = info.material.h.index;
		shaderDecal.opacity = Math::Clamp(info.opacity, 0.0f, 1.0f);
		// Fading over all of each half of the box is a fade depth of 1.
		shaderDecal.invFadeDepth = 2.0f / Math::Clamp(info.fadeDepth, 0.001f, 1.0f);
		shaderDecal.colourWeight = info.affectsColour ? 1.0f : 0.0f;
		shaderDecal.normalWeight = info.affectsNormal ? 1.0f : 0.0f;
		shaderDecal.roughnessWeight = info.affectsRoughness ? 1.0f : 0.0f;
		m_Renderer.GetRecords().decals.Set(handle.h.index, shaderDecal);
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
		// Checks the final CPU-side vertex array, common to both the ImGui and Nuklear
		// submission paths, before anything in the upload/GPU pipeline touches it.
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
		// big - this slot's own writes/reads are confined to its own slot, at this fixed byte
		// base.
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

	void RendererAPI::SubmitDebugLines(const DebugLineVertex* depthTested, uint depthTestedCount, const DebugLineVertex* onTop, uint onTopCount)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		TYR_ASSERT(renderFrame.debugLineDepthTestedCount + renderFrame.debugLineOnTopCount == 0);
		TYR_ASSERT(depthTestedCount + onTopCount <= RenderConstants::c_MaxDebugLineVertices);
		if (depthTestedCount + onTopCount == 0)
		{
			return;
		}

		const size_t depthTestedBytes = sizeof(DebugLineVertex) * depthTestedCount;
		const size_t onTopBytes = sizeof(DebugLineVertex) * onTopCount;
		UploadBufferAllocation alloc;
		if (!m_AllocManager.RequestFrameUploadAllocation(depthTestedBytes + onTopBytes, alloc))
		{
			return;
		}

		// Both lists go into one allocation, depth-tested first, so one copy uploads them.
		RenderBuffer& uploadBuffer = m_Registry.GetBuffer(alloc.buffer);
		if (depthTestedCount > 0)
		{
			RenderResourceUtil::WriteUploadBuffer(uploadBuffer, m_Device, alloc.offset, (void*)depthTested, depthTestedBytes);
		}
		if (onTopCount > 0)
		{
			RenderResourceUtil::WriteUploadBuffer(uploadBuffer, m_Device, alloc.offset + depthTestedBytes, (void*)onTop, onTopBytes);
		}

		BufferUploadRequest& request = renderFrame.frameBufferUploadRequests.ExpandOne();
		request.srcBuffer = alloc.buffer;
		request.srcOffset = alloc.offset;
		request.dstBuffer = m_Renderer.GetRenderResources().debugLineVertexBuffer;
		request.dstOffset = m_Renderer.GetRenderFrameIndex() * RenderConstants::c_DebugLineVertexBufferSize;
		request.size = depthTestedBytes + onTopBytes;

		renderFrame.debugLineDepthTestedCount = depthTestedCount;
		renderFrame.debugLineOnTopCount = onTopCount;
	}

	void RendererAPI::ResetGUIDrawData()
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		renderFrame.guiDrawData.Clear();
		renderFrame.guiVertexCursor = 0;
		renderFrame.guiIndexCursor = 0;
	}

	void RendererAPI::SetQualityLevel(QualityLevel level)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		renderFrame.hasQualityLevelOverride = true;
		renderFrame.qualityLevelOverride = level;
	}

	uint RendererAPI::RequestViewportCapture()
	{
		return m_Renderer.RequestViewportCapture();
	}

	ViewportCaptureStatus RendererAPI::TakeViewportCapture(uint id, Array<uint8>& outPixels, uint& outWidth, uint& outHeight)
	{
		return m_Renderer.TakeViewportCapture(id, outPixels, outWidth, outHeight);
	}

	void RendererAPI::SetTaaEnabled(bool enabled)
	{
		RenderFrame& renderFrame = m_Renderer.GetRenderFrame();
		renderFrame.hasTaaEnabledOverride = true;
		renderFrame.taaEnabledOverride = enabled;
		m_Renderer.SetTaaEnabledMainThread(enabled);
	}
}