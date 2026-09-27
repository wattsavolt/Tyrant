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
	struct GUIDrawData;

	// Every Create*/Delete* function here must only ever be called from the main thread -
	// RenderRegistry's pools aren't safe for concurrent creation/deletion against
	// RenderAsync's own reads of them on a worker thread.
	class TYR_RENDERER_API RendererAPI final : INonCopyable
	{
	public:
		RendererAPI(Renderer& renderer);
		~RendererAPI();

		RenderWindowHandle AddWindow(void* osHandle);

		void RemoveWindow(RenderWindowHandle window);

		void ResizeWindow(RenderWindowHandle window);

		SceneHandle AddScene(const char* name);

		void RemoveScene(SceneHandle handle);

		void SetActiveScene(SceneHandle handle, bool visible);

		void AddBufferUploadRequest(const BufferUploadRequest& request);

		void AddTextureUploadRequest(const TextureUploadRequest& request);

		// Creates and adds texture to the bindless array
		TextureHandle CreateTexture(const TextureDesc& desc);

		void DeleteTexture(TextureHandle handle);

		const TextureInfo& GetTextureInfo(TextureHandle handle);

		// Creates the editor viewport panel's offscreen colour render target on first call, or
		// resizes it (deleting the old one, creating a new one at the requested size) if
		// width/height differ from last time. Safe to call every frame from editor code - only
		// actually does work when the size has changed.
		TextureHandle GetOrCreateViewportTexture(const char* name, uint width, uint height);

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

		// Flat ambient term added to every pixel regardless of any light - see MeshPS.hlsl.
		// RenderFrame is per-frame buffered state (see AddView's own comment on the same
		// pattern), so this needs to be resupplied every frame too, not just once.
		void SetAmbient(float ambient);

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

		// Uploads one immediate-mode UI source's draw data (editor chrome, in-game HUD/menu) for
		// this frame, ready for GUIPass to render. A no-op if data has no vertices/indices.
		void SubmitGUIDrawData(const GUIDrawData& data);

		// Discards whatever GUI draw data is still sitting unrendered in the current render
		// frame slot - see GUIModule::EndFrame's call site for why this is needed.
		void ResetGUIDrawData();

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