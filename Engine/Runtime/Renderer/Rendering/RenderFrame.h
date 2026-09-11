#pragma once

#include "Rendering/Scene.h"
#include "RenderResource/Texture.h"
#include "RenderTransfer/UploadRequest.h"
#include "Rendering/RenderConstants.h"
#include "RenderInstance/RenderInstances.h"

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
		static constexpr uint c_InvalidSceneIndex = UINT32_MAX;
	
		// True if any window needs a resize
		bool windowResizeRequired = false;
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
		float deltaTime;
		uint activeSceneIndex;
		// Frame update for the active scene
		SceneFrame sceneFrame;
	
		RenderFrame()
			: activeSceneIndex(c_InvalidSceneIndex)
		{
			assetBufferUploadRequests.Reserve(128);
			textureUploadRequests.Reserve(128);
			fileToBufferUploadRequests.Reserve(128);
			fileToTextureUploadRequests.Reserve(128);
			texturesToAdd.Reserve(RenderConstants::c_MaxTextures / 5);
			texturesToDelete.Reserve(RenderConstants::c_MaxTextures);
			materialsToDelete.Reserve(RenderConstants::c_MaxMaterials);
			meshesToDelete.Reserve(RenderConstants::c_MaxMeshes);
			skeletalMeshesToDelete.Reserve(RenderConstants::c_MaxMeshes);
			meshInstancesToDelete.Reserve(RenderConstants::c_MaxMeshInstances);
			skeletalMeshInstancesToDelete.Reserve(RenderConstants::c_MaxSkeletalMeshInstances);
			dirLightsToDelete.Reserve(RenderConstants::c_MaxDirLights);
			pointLightsToDelete.Reserve(RenderConstants::c_MaxPointLights);
			spotLightsToDelete.Reserve(RenderConstants::c_MaxSpotLights);
		}

		void Clear()
		{
			windowResizeRequired = false;
			assetBufferUploadRequests.Clear();
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
			sceneFrame.Clear();
			activeSceneIndex = c_InvalidSceneIndex;
		}
	};
}