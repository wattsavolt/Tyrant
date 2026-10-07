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
	struct ViewportGridDesc;
	enum class QualityLevel : uint8;

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

		// Just allocates a pool slot - creation doesn't also create any textures yet. Call once
		// per scene and assign the result to that scene via SetSceneRenderViewport.
		RenderViewportHandle CreateRenderViewport();

		void DeleteRenderViewport(RenderViewportHandle viewport);

		// Takes an explicit scene handle (unlike SetActiveScene/AddView/etc) so it can update
		// ImmediateSceneData for the right scene regardless of whether that scene is active yet.
		// Also queues the merge RenderAsync applies to Scene::renderViewport proper.
		void SetSceneRenderViewport(SceneHandle scene, RenderViewportHandle viewport);

		// visible is written straight into ImmediateSceneData - RenderAsync never reads it, so
		// there's no SceneFrame merge involved for it, unlike activeScene itself.
		void SetActiveScene(SceneHandle handle, bool visible);

		void AddBufferUploadRequest(const BufferUploadRequest& request);

		void AddTextureUploadRequest(const TextureUploadRequest& request);

		// Creates and adds texture to the bindless array
		TextureHandle CreateTexture(const TextureDesc& desc);

		void DeleteTexture(TextureHandle handle);

		const TextureInfo& GetTextureInfo(TextureHandle handle);

		// Creates viewport's offscreen colour render target (plus its G-buffer/depth targets) for
		// this tick's own buffered slot on first call, or resizes them if width/height differ from
		// last time. Safe to call every frame - only actually does work when the size has changed.
		TextureHandle GetOrCreateRenderViewportTexture(RenderViewportHandle viewport, const char* name, uint width, uint height);

		// Draws an infinite grid over the viewport's final image, e.g. for a level editor.
		void SetRenderViewportGrid(RenderViewportHandle viewport, const ViewportGridDesc& desc);

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

		// Queues a one-time build of this mesh's bottom-level acceleration structure, from LOD0's
		// already-allocated vertex/index buffer ranges. A no-op if called again for a mesh that
		// already has one.
		void RequestBLASBuild(MeshHandle handle);

		MeshInstanceHandle CreateMeshInstance(const MeshInstanceDesc& desc);

		void UpdateMeshInstance(MeshInstanceHandle handle, const MeshInstanceDesc& desc);

		void DeleteMeshInstance(MeshInstanceHandle handle);


		// TODO: Add create, update and delete functions for skeletal mesh instances here
		
		// Takes an explicit scene handle, independent of which scene is currently active.
		void SetSceneWindow(SceneHandle scene, RenderWindowHandle window);

		// Adds a view for the next frame. Must be called for each view every frame
		void AddView(const SceneView& view);

		// Flat ambient term added to every pixel regardless of any light. Written straight into
		// ImmediateSceneData - unlike SetSceneWindow/SetSceneRenderViewport, RenderAsync never
		// reads this, so there's no SceneFrame merge to also queue.
		void SetSceneAmbient(SceneHandle scene, float ambient);

		// Takes an explicit scene handle so the light goes in that scene's light list.
		DirLightHandle CreateDirectionalLight(SceneHandle scene, const DirectionalLightDesc& desc);

		void UpdateDirectionalLight(DirLightHandle handle, const DirectionalLightDesc& desc);

		void DeleteDirectionalLight(SceneHandle scene, DirLightHandle handle);

		PointLightHandle CreatePointLight(SceneHandle scene, const PointLightDesc& desc);

		void UpdatePointLight(PointLightHandle handle, const PointLightDesc& desc);

		void DeletePointLight(SceneHandle scene, PointLightHandle handle);

		SpotLightHandle CreateSpotLight(SceneHandle scene, const SpotLightDesc& desc);

		void UpdateSpotLight(SpotLightHandle handle, const SpotLightDesc& desc);

		void DeleteSpotLight(SceneHandle scene, SpotLightHandle handle);

		bool RequestResourceUploadAllocation(size_t size, UploadBufferAllocation& allocation);

		void FlushBufferUploadAllocation(const UploadBufferAllocation& alloc);

		// Uploads one immediate-mode UI source's draw data (editor chrome, in-game HUD/menu) for
		// this frame, ready for GUIPass to render. A no-op if data has no vertices/indices.
		void SubmitGUIDrawData(const GUIDrawData& data);

		// Discards whatever GUI draw data is still sitting unrendered in the current render
		// frame slot.
		void ResetGUIDrawData();

		// Takes effect from next tick's RenderAsync onward - queued through RenderFrame the same
		// one-shot way SetSceneRenderViewport is, since RenderAsync runs on a worker thread.
		void SetQualityLevel(QualityLevel level);

		// Independent of SetQualityLevel - TAA's own on/off switch isn't tied to any quality
		// preset, so it can be flipped on its own to isolate whether it's responsible for a
		// visual issue. Also updates a main-thread-only mirror immediately (unlike the worker-
		// owned override queued above), since the editor's own viewport display needs to know
		// which texture to show without waiting for RenderAsync to catch up.
		void SetTaaEnabled(bool enabled);

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