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

		// Just allocates a pool slot (see RenderViewport's own comment on why creation doesn't also
		// create any textures yet) - call once per scene (see WorldManager::InitWorld) and assign
		// the result to that scene via SetSceneRenderViewport.
		RenderViewportHandle CreateRenderViewport();

		void DeleteRenderViewport(RenderViewportHandle viewport);

		// Takes an explicit scene handle (unlike SetActiveScene/AddView/etc, which implicitly mean
		// "this tick's active scene") so it can update ImmediateSceneData for the right scene
		// regardless of whether that scene is active yet - see ImmediateSceneData's own comment.
		// Also queues the merge RenderAsync applies to Scene::renderViewport proper, same as
		// SetSceneWindow does for windowHandle (see SceneFrame::newRenderViewport) - that merge is
		// still scoped to whichever scene is active when RenderAsync processes it, so this should
		// only be called for a scene at or before the same tick it becomes active.
		void SetSceneRenderViewport(SceneHandle scene, RenderViewportHandle viewport);

		// visible is written straight into ImmediateSceneData (see its own comment) - like
		// ambient, RenderAsync never reads it, so there's no SceneFrame merge involved for it,
		// unlike activeScene itself (still plain per-frame RenderFrame state - see AddView's own
		// comment on why that still needs resupplying every frame).
		void SetActiveScene(SceneHandle handle, bool visible);

		void AddBufferUploadRequest(const BufferUploadRequest& request);

		void AddTextureUploadRequest(const TextureUploadRequest& request);

		// Creates and adds texture to the bindless array
		TextureHandle CreateTexture(const TextureDesc& desc);

		void DeleteTexture(TextureHandle handle);

		const TextureInfo& GetTextureInfo(TextureHandle handle);

		// Creates viewport's offscreen colour render target (plus its G-buffer/depth targets) for
		// this tick's own buffered slot on first call, or resizes them if width/height differ from
		// last time - see RenderViewport's own comment on how a resize propagates to the other
		// buffered slots. Safe to call every frame from editor code - only actually does work when
		// the size has changed.
		TextureHandle GetOrCreateRenderViewportTexture(RenderViewportHandle viewport, const char* name, uint width, uint height);

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

		// Queues a one-time build of this mesh's bottom-level acceleration structure (BLAS),
		// from LOD0's already-allocated vertex/index buffer ranges - call once LOD0's geometry
		// upload has actually been queued (see AssetManager::UploadMeshGeometry), not just
		// allocated, so the build's implicit ordering against that upload is correct. A no-op
		// safety net if called again for a mesh that already has one.
		void RequestBLASBuild(MeshHandle handle);

		MeshInstanceHandle CreateMeshInstance(const MeshInstanceDesc& desc);

		void UpdateMeshInstance(MeshInstanceHandle handle, const MeshInstanceDesc& desc);

		void DeleteMeshInstance(MeshInstanceHandle handle);


		// TODO: Add create, update and delete functions for skeletal mesh instances here
		
		// Takes an explicit scene handle - see SetSceneRenderViewport's own comment for why.
		void SetSceneWindow(SceneHandle scene, RenderWindowHandle window);

		// Adds a view for the next frame. Must be called for each view every frame
		void AddView(const SceneView& view);

		// Flat ambient term added to every pixel regardless of any light - see MeshPS.hlsl. Written
		// straight into ImmediateSceneData (see its own comment) - unlike SetSceneWindow/
		// SetSceneRenderViewport, RenderAsync never reads this, so there's no SceneFrame merge to
		// also queue. Persists once set, like windowHandle/renderViewport - no need to resupply it
		// every frame.
		void SetSceneAmbient(SceneHandle scene, float ambient);

		// Takes an explicit scene handle (see SetSceneRenderViewport's own comment) so
		// ImmediateSceneData's light counts stay correct per scene.
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
		// frame slot - see GUIModule::Update's call site for why this is needed.
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