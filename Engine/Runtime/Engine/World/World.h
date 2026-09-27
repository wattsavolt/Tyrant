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
		// The OS window backing windowHandle above. Lets WorldManager poll WindowModule each tick
		// for an externally (OS-)detected resize and forward it to RendererAPI::ResizeWindow -
		// see WorldManager::UpdateWorld.
		WindowHandle osWindowHandle;
		ViewArea viewArea;
		// The world is provided the camera (will be a component later) but its dimensions will be updated by the world manager when the window resizes
		Camera* camera = nullptr;
	};

	// One MeshComponent's renderer-side mesh instance, tracked outside the ECS so
	// WorldManager can delete it (and release the world's reference to the underlying mesh
	// asset) when the world is torn down - see WorldManager::UpdateWorld/ShutdownWorld.
	// meshInstance stays default (invalid/falsy) until AssetManager::CreateMeshInstance's
	// onCreated callback fires - creation is asynchronous, so there's a window where this
	// entry exists but the instance doesn't yet.
	struct WorldMeshInstance
	{
		AssetID meshAssetID;
		// Only actually populated once meshInstance itself resolves (see its own comment) -
		// the per-slot material AssetIDs AssetManager::CreateMeshInstance resolved and loaded
		// on this instance's behalf, needed so WorldManager::ShutdownWorld can release this
		// instance's share of each one's refcount alongside the mesh's own.
		LocalArray<AssetID, MeshConstants::c_MaxSubmeshes> materialAssetIDs;
		MeshInstanceHandle meshInstance;
	};

	// A world / scene in an app. Just data - WorldManager owns all the logic that
	// creates, updates, and tears one of these down, and callers reach a world through
	// WorldManager via its handle rather than calling anything on World itself. Whether a
	// world is "active" isn't state a World carries itself - only one world's scene ever
	// renders at a time, so WorldManager tracks that single active world directly.
	struct World
	{
		Name name;
		Camera* camera = nullptr;
		ViewArea viewArea;
		RenderWindowHandle windowHandle;
		WindowHandle osWindowHandle;
		SceneHandle sceneHandle;
		bool visible = true;
		EntitySystem entities;
		Array<WorldMeshInstance> meshInstances;
		// One-shot: set the first time WorldManager::UpdateWorld creates mesh instances for
		// this world's MeshComponents, so it only ever does that once. There's no runtime
		// entity add/remove yet (see EntitySystem's own comment on ForEach), so a one-time
		// sync is enough for now - revisit once something actually adds entities after a
		// world's already running.
		bool meshInstancesSynced = false;
		// Renderer-side directional lights created from this world's DirLightComponents - see
		// WorldManager::UpdateWorld/ShutdownWorld.
		Array<DirLightHandle> dirLights;
		bool dirLightsSynced = false;

		// Puts a recycled pool slot back to a blank state before WorldManager::InitWorld
		// fills it in again for a new world. entities.Reset() drops every entity/archetype
		// from whatever level was previously using this slot - without it, a recycled World
		// would start a new level still carrying the last one's entities.
		void Reset()
		{
			name = {};
			camera = nullptr;
			viewArea = {};
			windowHandle = {};
			osWindowHandle = {};
			sceneHandle = {};
			visible = true;
			entities.Reset();
			meshInstances.Clear();
			meshInstancesSynced = false;
			dirLights.Clear();
			dirLightsSynced = false;
		}
	};
}
