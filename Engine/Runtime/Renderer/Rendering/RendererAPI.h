#pragma once

#include "Core.h"
#include "RendererMacros.h"
#include "RenderBase/RenderHandles.h"

namespace tyr
{
	class Device;
	class Renderer;
	class RenderRegistry;
	class RenderAllocationManager;
	struct UploadBufferAllocation;
	struct BufferUploadRequest;
	struct TextureUploadRequest;
	struct GpuBufferAllocation;
	struct TextureInfo;
	struct TextureDesc;
	struct MaterialDesc;
	struct MeshDesc;
	struct MeshInstanceDesc;
	struct MeshInstanceInfo;
	struct SceneView;
	struct DirectionalLightDesc;
	struct PointLightDesc;
	struct SpotLightDesc;
	struct DirectionalLightInfo;
	struct PointLightInfo;
	struct SpotLightInfo;

	class TYR_RENDERER_API RendererAPI final : INonCopyable
	{
	public:
		RendererAPI(Renderer& renderer);
		~RendererAPI();

		RenderWindowHandle AddWindow(void* osHandle);

		void RemoveWindow(RenderWindowHandle window);

		void ResizeWindow(RenderWindowHandle window, uint width, uint height);

		uint AddScene(const char* name);

		void RemoveScene(uint index);
	
		void SetActiveSceneIndex(uint index, bool visible);

		void AddBufferUploadRequest(const BufferUploadRequest& request);

		void AddTextureUploadRequest(const TextureUploadRequest& request);

		// Creates and adds texture to the bindless array
		TextureHandle CreateTexture(const TextureDesc& desc);

		void DeleteTexture(TextureHandle handle);

		const TextureInfo& GetTextureInfo(TextureHandle handle);

		MaterialHandle CreateMaterial(const MaterialDesc& desc);

		void DeleteMaterial(MaterialHandle handle);

		MeshHandle CreateMesh(const MeshDesc& desc);

		bool RequestMeshLODBufferAllocation(MeshHandle handle, uint lodIndex, GpuBufferAllocation& allocation);

		bool RequestVertexBufferAllocation(MeshHandle handle, uint lodIndex, size_t size, GpuBufferAllocation& allocation);

		bool RequestIndexBufferAllocation(MeshHandle handle, uint lodIndex, size_t size, GpuBufferAllocation& allocation);

		bool RequestMeshletBufferAllocation(MeshHandle handle, uint lodIndex, size_t size, GpuBufferAllocation& allocation);

		// Frees vertex, index and meshlet allocations referenced by the mesh lod
		void FreeMeshLODAllocations(MeshHandle handle, uint lodIndex);

		void DeleteMesh(MeshHandle handle);

		MeshInstanceHandle CreateMeshInstance(const MeshInstanceDesc& desc);

		void UpdateMeshInstance(MeshInstanceHandle handle, const MeshInstanceDesc& desc);

		void DeleteMeshInstance(MeshInstanceHandle handle);


		// TODO: Add create, update and delete functions for skeletal mesh instances here
		
		void SetSceneWindow(RenderWindowHandle window);

		// Adds a view for the next frame. Must be called for each view every frame
		void AddView(const SceneView& view);

		DirLightHandle CreateDirectionalLight(const DirectionalLightDesc& desc);

		void UpdateDirectionalLight(DirLightHandle handle, const DirectionalLightDesc& desc);

		void DeleteDirectionalLight(DirLightHandle handle);

		PointLightHandle CreatePointLight(const PointLightDesc& desc);

		void UpdatePointLight(PointLightHandle handle, const PointLightDesc& desc);

		void DeletePointLight(PointLightHandle handle);

		SpotLightHandle CreateSpotLight(const SpotLightDesc& desc);

		void UpdateSpotLight(SpotLightHandle handle, const SpotLightDesc& desc);

		void DeleteSpotLight(SpotLightHandle handle);

		bool RequestResourceUploadAllocation(size_t size, UploadBufferAllocation& allocation);

		void FlushBufferUploadAllocation(const UploadBufferAllocation& alloc);

	private:
		void UploadMeshInstance(const MeshInstanceInfo& info, uint index);
		void UploadDirectionalLight(const DirectionalLightInfo& info, uint index);
		void UploadPointLight(const PointLightInfo& info, uint index);
		void UploadSpotLight(const SpotLightInfo& info, uint index);

		Renderer& m_Renderer;
		RenderRegistry& m_Registry;
		RenderAllocationManager& m_AllocManager;
		Device& m_Device;
	};
}