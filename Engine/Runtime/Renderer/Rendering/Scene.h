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

	// A main-thread-only mirror of the handful of Scene fields Renderer::Render() (main thread)
	// itself needs to read every tick - e.g. to look up the swap chain's size, apply a viewport
	// resize, or know how many lights to report in this tick's scene info upload. Scene (in
	// RenderData::scenePool) is worker-owned: only RenderAsync (a worker thread) ever reads/writes
	// it, since it needs that exclusivity for its own merge logic (mesh instances, lights, etc.) -
	// so Render() reading Scene::windowHandle/renderViewport/content directly would be a genuine
	// cross-thread race against that same worker's writes. This struct is instead updated
	// synchronously, in place, by whichever RendererAPI call also queues the matching SceneFrame
	// update for RenderAsync to merge into Scene proper (see RendererAPI::SetSceneWindow/
	// SetSceneRenderViewport/CreateDirectionalLight etc.) - by the time Render() reads it, it's
	// always already current for this tick, with no "this tick's fresh value, or else the
	// persisted one" fallback needed. ambient/visible need no such merge at all - RenderAsync never
	// reads either, so RendererAPI::SetSceneAmbient/SetActiveScene just write here directly.
	//
	// One entry per scene (see Renderer::m_ImmediateSceneData), indexed the same way
	// RenderData::scenePool is - created alongside a scene in RendererAPI::AddScene.
	struct ImmediateSceneData
	{
		RenderWindowHandle windowHandle{};
		RenderViewportHandle renderViewport{};
		// Kept up to date by RendererAPI::CreateDirectionalLight/DeleteDirectionalLight (and the
		// Point/Spot equivalents) incrementing/decrementing directly, instead of Render() reading
		// Scene::content.dirLights.Size() etc. - see this struct's own comment.
		uint dirLightCount = 0;
		uint pointLightCount = 0;
		uint spotLightCount = 0;
		// Flat ambient term added to every pixel regardless of any light - see MeshPS.hlsl and
		// RendererAPI::SetSceneAmbient.
		float ambient = 0.0f;
		// Whether this scene should actually render/upload this tick - see RendererAPI::
		// SetActiveScene and Renderer::Render's own use of this.
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
		// Set via RendererAPI::SetSceneRenderViewport, merged in by RenderAsync the same way
		// windowHandle is (see SceneFrame::newRenderViewport) - no two scenes ever share one, and
		// this is the only place a scene's RenderViewportHandle lives (see RenderViewport's own
		// comment on why it isn't in RenderRegistry).
		RenderViewportHandle renderViewport{};
		uint id{};
		LocalArray<SceneView, RenderConstants::c_MaxViewsPerScene> views;
		SceneContent content;

		void Clear()
		{
			views.Clear();
			content.Clear();
		}

		// Puts a recycled pool slot back to a blank state - see RenderData::scenes. Reuses
		// Clear() for the containers (keeps their reserved capacity, unlike a plain `= {}`
		// would) and additionally resets the identity fields Clear() intentionally leaves alone
		// during normal per-frame use.
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