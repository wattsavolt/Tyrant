#pragma once

#include "Rendering/Scene.h"
#include "Rendering/GUIDrawData.h"
#include "RenderResource/Texture.h"
#include "RenderTransfer/UploadRequest.h"
#include "Rendering/RenderConstants.h"
#include "RenderInstance/RenderInstances.h"
#include "RenderWindow.h"

namespace tyr
{
	struct MeshInstanceUpdate
	{
		MeshInstanceHandle handle;
		MeshInstanceDesc desc;
	};

	struct DirLightUpdate
	{
		DirLightHandle handle;
		DirectionalLightDesc desc;
	};

	struct PointLightUpdate
	{
		PointLightHandle handle;
		PointLightDesc desc;
	};

	struct SpotLightUpdate
	{
		SpotLightHandle handle;
		SpotLightDesc desc;
	};

	struct SceneFrame
	{
		LocalArray<SceneView, RenderConstants::c_MaxViewsPerScene> views;
		Array<MeshInstanceHandle> meshInstancesToAdd;
		Array<MeshInstanceUpdate> meshInstancesToUpdate;
		Array<MeshInstanceHandle> meshInstancesToRemove;
		Array<SkeletalMeshInstanceHandle> skeletalMeshInstancesToAdd;
		Array<SkeletalMeshInstanceHandle> skeletalMeshInstancesToUpdate;
		Array<SkeletalMeshInstanceHandle> skeletalMeshInstancesToRemove;
		Array<DirLightHandle> dirLightsToAdd;
		Array<DirLightUpdate> dirLightsToUpdate;
		Array<DirLightHandle> dirLightsToRemove;
		Array<PointLightHandle> pointLightsToAdd;
		Array<PointLightUpdate> pointLightsToUpdate;
		Array<PointLightHandle> pointLightsToRemove;
		Array<SpotLightHandle> spotLightsToAdd;
		Array<SpotLightUpdate> spotLightsToUpdate;
		Array<SpotLightHandle> spotLightsToRemove;
		RenderWindowHandle newWindow{};
		float ambient{};
		bool visible{};

		SceneFrame()
		{
			meshInstancesToAdd.Reserve(RenderConstants::c_MaxMeshInstances);
			meshInstancesToUpdate.Reserve(RenderConstants::c_MaxMeshInstances);
			meshInstancesToRemove.Reserve(RenderConstants::c_MaxMeshInstances);
			skeletalMeshInstancesToAdd.Reserve(RenderConstants::c_MaxSkeletalMeshInstances);
			skeletalMeshInstancesToUpdate.Reserve(RenderConstants::c_MaxSkeletalMeshInstances);
			skeletalMeshInstancesToRemove.Reserve(RenderConstants::c_MaxSkeletalMeshInstances);
			dirLightsToAdd.Reserve(RenderConstants::c_MaxDirLights);
			dirLightsToUpdate.Reserve(RenderConstants::c_MaxDirLights);
			dirLightsToRemove.Reserve(RenderConstants::c_MaxDirLights);
			pointLightsToAdd.Reserve(RenderConstants::c_MaxPointLights);
			pointLightsToUpdate.Reserve(RenderConstants::c_MaxPointLights);
			pointLightsToRemove.Reserve(RenderConstants::c_MaxPointLights);
			spotLightsToAdd.Reserve(RenderConstants::c_MaxSpotLights);
			spotLightsToUpdate.Reserve(RenderConstants::c_MaxSpotLights);
			spotLightsToRemove.Reserve(RenderConstants::c_MaxSpotLights);
		}

		void Clear()
		{
			// Clear and overwrite each frame
			views.Clear();
			meshInstancesToAdd.Clear();
			meshInstancesToUpdate.Clear();
			meshInstancesToRemove.Clear();
			skeletalMeshInstancesToAdd.Clear();
			skeletalMeshInstancesToUpdate.Clear();
			skeletalMeshInstancesToRemove.Clear();
			dirLightsToAdd.Clear();
			dirLightsToUpdate.Clear();
			dirLightsToRemove.Clear();
			pointLightsToAdd.Clear();
			pointLightsToUpdate.Clear();
			pointLightsToRemove.Clear();
			spotLightsToAdd.Clear();
			spotLightsToUpdate.Clear();
			spotLightsToRemove.Clear();
			visible = true;
			newWindow = {};
		}
	};

	struct RenderWindowResizeRequest
	{
		RenderWindowHandle window{};
	};

	struct RenderFrame
	{
		// A handful of adds/deletes per frame is typical - reserving each list below to its
		// resource type's absolute max (some in the thousands) would waste a lot of memory
		// for a case this small, repeated across every buffered slot. Array grows on its own
		// in the rare frame that actually queues more than this.
		static constexpr uint c_DefaultPendingListReserve = 16;

		Array<BufferUploadRequest> assetBufferUploadRequests;
		Array<BufferUploadRequest> frameBufferUploadRequests;
		Array<TextureUploadRequest> textureUploadRequests;
		Array<FileToBufferUploadRequest> fileToBufferUploadRequests;
		Array<FileToTextureUploadRequest> fileToTextureUploadRequests;
		Array<TextureHandle> texturesToAdd;
		// Resources to be deleted when the frame has finished rendering
		Array<TextureHandle> texturesToDelete;
		Array<MaterialHandle> materialsToDelete;
		Array<MeshHandle> meshesToDelete;
		Array<SkeletalMeshHandle> skeletalMeshesToDelete;
		Array<MeshInstanceHandle> meshInstancesToDelete;
		Array<SkeletalMeshInstanceHandle> skeletalMeshInstancesToDelete;
		Array<DirLightHandle> dirLightsToDelete;
		Array<PointLightHandle> pointLightsToDelete;
		Array<SpotLightHandle> spotLightsToDelete;
		// Windows removed this tick. A window's swap chain/semaphores can't be deleted right
		// away - RenderAsync or RenderSubmissionThread might still be using them from up to
		// c_BufferedFrameCount frames ago - so, like every list above, actual deletion waits
		// until this slot cycles back around and its GPU work is confirmed done. The pool handle
		// travels with each entry too, so the pool slot itself is freed at that same safe point
		// rather than immediately (see RemoveWindow's own comment).
		Array<PendingWindowDelete> windowsToDelete;
		// Scenes removed this tick - deferred the same way and for the same reason as windows
		// above (see RendererAPI::RemoveScene). Unlike windows, nothing needs copying out first:
		// Scene::Reset() runs in place once it's safe, so this just needs the handle.
		Array<SceneHandle> scenesToDelete;
		float deltaTime;
		// Default-constructs to invalid (falsy) - no separate sentinel needed.
		SceneHandle activeScene;
		// Frame update for the active scene
		SceneFrame sceneFrame;

		// One entry per RendererAPI::SubmitGUIDrawData call this frame - editor chrome (ImGui)
		// and an in-game HUD/menu (Nuklear) can both submit in the same frame.
		Array<GUIDrawSubmission> guiDrawData;
		// Where in the shared GUI vertex/index buffers this frame's next SubmitGUIDrawData call
		// should write to - advances as each submission is uploaded, reset to 0 in Clear().
		uint guiVertexCursor = 0;
		uint guiIndexCursor = 0;

		RenderFrame()
		{
			assetBufferUploadRequests.Reserve(128);
			textureUploadRequests.Reserve(128);
			fileToBufferUploadRequests.Reserve(128);
			fileToTextureUploadRequests.Reserve(128);
			texturesToAdd.Reserve(c_DefaultPendingListReserve);
			texturesToDelete.Reserve(c_DefaultPendingListReserve);
			materialsToDelete.Reserve(c_DefaultPendingListReserve);
			meshesToDelete.Reserve(c_DefaultPendingListReserve);
			skeletalMeshesToDelete.Reserve(c_DefaultPendingListReserve);
			meshInstancesToDelete.Reserve(c_DefaultPendingListReserve);
			skeletalMeshInstancesToDelete.Reserve(RenderConstants::c_MaxSkeletalMeshInstances);
			dirLightsToDelete.Reserve(RenderConstants::c_MaxDirLights);
			pointLightsToDelete.Reserve(RenderConstants::c_MaxPointLights);
			spotLightsToDelete.Reserve(RenderConstants::c_MaxSpotLights);
		}

		void Clear()
		{
			assetBufferUploadRequests.Clear();
			frameBufferUploadRequests.Clear();
			textureUploadRequests.Clear();
			fileToBufferUploadRequests.Clear();
			fileToTextureUploadRequests.Clear();
			texturesToAdd.Clear();
			texturesToDelete.Clear();
			materialsToDelete.Clear();
			meshesToDelete.Clear();
			skeletalMeshesToDelete.Clear();
			meshInstancesToDelete.Clear();
			skeletalMeshInstancesToDelete.Clear();
			dirLightsToDelete.Clear();
			pointLightsToDelete.Clear();
			spotLightsToDelete.Clear();
			windowsToDelete.Clear();
			scenesToDelete.Clear();
			sceneFrame.Clear();
			activeScene = {};
			guiDrawData.Clear();
			guiVertexCursor = 0;
			guiIndexCursor = 0;
		}
	};
}