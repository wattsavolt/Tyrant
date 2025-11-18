#pragma once

#include "RendererMacros.h"
#include "Core.h"
#include "Math/Vector3.h"
#include "Math/Quaternion.h"
#include "Math/Matrix4.h"
#include "RenderAPI/RenderAPITypes.h"
#include "RenderDataTypes/RenderDataTypes.h"

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
		SceneViewArea viewArea;
		SceneCamera camera;
	};

	constexpr uint c_MaxDirectionalLights = 3;
	constexpr uint c_MaxPointLights = 20;
	constexpr uint c_MaxSpotLights = 10;

	struct SceneContent
	{
		Array<RigidMeshInstance> rigidMeshInstances;
		Array<SkeletalMeshInstance> skeletalModelInstances;
		LocalArray<DirectionalLight, c_MaxDirectionalLights> dirLights;
		LocalArray<PointLight, c_MaxPointLights> pointLights;
		LocalArray<SpotLight, c_MaxSpotLights> spotLights;
	};

	struct Scene
	{
#if TYR_EDITOR
		static constexpr uint8 c_MaxScenes = 4;
#else
		static constexpr uint8 c_MaxScenes = 1;
#endif
		uint8 id;
		bool active;
		bool visible;
		const char* name;
		SceneView view;
		SceneContent content;
	};

	struct SceneFrame
	{
		bool visible = true;
		SceneView view;
	};
}