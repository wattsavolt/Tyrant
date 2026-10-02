#pragma once

#include "Core.h"
#include "Math/Vector3.h"
#include "Math/Matrix4.h"
#include "EngineMacros.h"
#include "Rendering/Scene.h"
#include "AssetSystem/AssetDataTypes.h"
#include "ECS/EntitySystem.h"
#include "Window/WindowHandle.h"

namespace tyr
{
	class Camera;

	struct WorldConfig
	{
		Name name;
		// The render window the world's scene will draw to. Must come from RendererAPI::AddWindow.
		RenderWindowHandle windowHandle;
		// The OS window backing windowHandle above. Lets WorldManager poll for an externally
		// (OS-)detected resize each tick and forward it to the renderer.
		WindowHandle osWindowHandle;
		ViewArea viewArea;
		// The world is provided the camera (will be a component later) but its dimensions will be updated by the world manager when the window resizes
		Camera* camera = nullptr;
	};

	// One MeshComponent's renderer-side mesh instance, tracked outside the ECS so it can be
	// deleted (and release the world's reference to the mesh asset) when the world is torn
	// down. meshInstance stays invalid until creation resolves, since creation is asynchronous.
	struct WorldMeshInstance
	{
		AssetID meshAssetID;
		// Only actually populated once meshInstance itself resolves - the per-slot material
		// AssetIDs resolved and loaded on this instance's behalf, needed to release each
		// one's refcount alongside the mesh's own when torn down.
		LocalArray<AssetID, MeshConstants::c_MaxSubmeshes> materialAssetIDs;
		MeshInstanceHandle meshInstance;
	};

	// A world/scene in an app. Just data - a manager owns all the logic that creates, updates,
	// and tears one down. Whether a world is "active" isn't state it carries itself, since
	// only one world's scene ever renders at a time.
	struct World
	{
		Name name;
		Camera* camera = nullptr;
		ViewArea viewArea;
		RenderWindowHandle windowHandle;
		WindowHandle osWindowHandle;
		SceneHandle sceneHandle;
		// Created/destroyed alongside sceneHandle - the world's scene owns this for its whole
		// lifetime, regardless of whether this world is currently the active one.
		RenderViewportHandle renderViewportHandle;
		bool visible = true;
		EntitySystem entities;
		Array<WorldMeshInstance> meshInstances;
		// One-shot: set the first time mesh instances are created for this world's
		// MeshComponents, so it only happens once. Revisit once entities can be added after a
		// world is already running.
		bool meshInstancesSynced = false;
		// Renderer-side directional lights created from this world's DirLightComponents.
		Array<DirLightHandle> dirLights;
		bool dirLightsSynced = false;

		// Puts a recycled pool slot back to a blank state before it's filled in again for a
		// new world. entities.Reset() drops every entity/archetype from whatever level was
		// previously using this slot.
		void Reset()
		{
			name = {};
			camera = nullptr;
			viewArea = {};
			windowHandle = {};
			osWindowHandle = {};
			sceneHandle = {};
			renderViewportHandle = {};
			visible = true;
			entities.Reset();
			meshInstances.Clear();
			meshInstancesSynced = false;
			dirLights.Clear();
			dirLightsSynced = false;
		}
	};
}
