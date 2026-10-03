#pragma once

#include "RendererMacros.h"
#include "Core.h"
#include "Math/Vector3.h"
#include "RenderAPI/RenderAPITypes.h"
#include "Rendering/RenderConstants.h"
#include "RenderBase/RenderHandles.h"

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

	// A main-thread-only mirror of the handful of Scene fields Render() needs every tick. Scene
	// itself is worker-owned (only RenderAsync writes it), so reading it directly would race
	// those writes - this struct is updated synchronously instead.
	struct ImmediateSceneData
	{
		RenderWindowHandle windowHandle{};
		RenderViewportHandle renderViewport{};
		// Kept up to date by incrementing/decrementing directly, instead of reading
		// Scene::content.dirLights.Size() etc., which would race the worker thread's writes.
		uint dirLightCount = 0;
		uint pointLightCount = 0;
		uint spotLightCount = 0;
		// Flat ambient term added to every pixel regardless of any light.
		float ambient = 0.0f;
		// Whether this scene should actually render/upload this tick.
		bool visible = true;

		void Reset()
		{
			windowHandle = {};
			renderViewport = {};
			dirLightCount = 0;
			pointLightCount = 0;
			spotLightCount = 0;
			ambient = 0.0f;
			visible = true;
		}
	};

	// Note: There can be multiple scenes but only one scene will be rendered at a time
	struct Scene
	{
		const char* name{};
		RenderWindowHandle windowHandle{};
		// Set via SetSceneRenderViewport, merged in by RenderAsync the same way windowHandle is -
		// no two scenes ever share one, and this is the only place a scene's RenderViewportHandle
		// lives.
		RenderViewportHandle renderViewport{};
		uint id{};
		LocalArray<SceneView, RenderConstants::c_MaxViewsPerScene> views;
		SceneContent content;

		void Clear()
		{
			views.Clear();
			content.Clear();
		}

		// Puts a recycled pool slot back to a blank state. Reuses Clear() for the containers
		// (keeps their reserved capacity, unlike a plain `= {}` would) and additionally resets
		// the identity fields Clear() intentionally leaves alone.
		void Reset()
		{
			name = {};
			windowHandle = {};
			renderViewport = {};
			id = 0;
			Clear();
		}
	};
}