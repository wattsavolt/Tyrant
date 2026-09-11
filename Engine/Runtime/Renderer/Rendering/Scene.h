#pragma once

#include "RendererMacros.h"
#include "Core.h"
#include "Math/Vector3.h"
#include "RenderAPI/RenderAPITypes.h"
#include "Rendering/RenderConstants.h"
#include "RenderBase/RenderHandles.h"
#include "RenderTransfer/UploadRequest.h"

namespace tyr
{
	struct SceneCamera
	{
		Vector3 position;
		Vector3 forward;
		Vector3 up;
		// Field of view in degrees
		float fov;
		float nearZ;
		float farZ;
	};

	struct SceneView
	{
		SceneCamera camera;
		// Relative to the scene view area
		ViewArea viewArea;
		uint id;
	};

	struct SceneContent
	{
		Array<MeshInstanceHandle> meshInstances;
		Array<SkeletalMeshInstanceHandle> skeletalMeshInstances;
		Array<DirLightHandle> dirLights;
		Array<PointLightHandle> pointLights;
		Array<SpotLightHandle> spotLights;
	
		SceneContent()
		{
			meshInstances.Reserve(RenderConstants::c_MaxMeshInstances);
			skeletalMeshInstances.Reserve(RenderConstants::c_MaxSkeletalMeshInstances);
			dirLights.Reserve(RenderConstants::c_MaxDirLights);
			pointLights.Reserve(RenderConstants::c_MaxPointLights);
			spotLights.Reserve(RenderConstants::c_MaxSpotLights);
		}

		void Clear()
		{
			meshInstances.Clear();
			skeletalMeshInstances.Clear();
			dirLights.Clear();
			pointLights.Clear();
			spotLights.Clear();
		}
	};

	// Note: There can be multiple scenes but only one scene will be rendered at a time
	struct Scene
	{
		const char* name{};
		RenderWindowHandle windowHandle{};
		uint id;
		bool inUse = false;
		Array<BufferUploadRequest> frameUploadRequests;
		LocalArray<SceneView, RenderConstants::c_MaxViewsPerScene> views;
		SceneContent content;

		Scene()
		{
			frameUploadRequests.Reserve(128);
		}

		void Clear()
		{
			frameUploadRequests.Clear();
			views.Clear();
			content.Clear();
		}
	};
}