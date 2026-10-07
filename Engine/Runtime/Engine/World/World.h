#pragma once

#include "Core.h"
#include "Math/Vector3.h"
#include "Math/Matrix4.h"
#include "EngineMacros.h"
#include "Rendering/Scene.h"
#include "AssetSystem/AssetDataTypes.h"
#include "ECS/EntitySystem.h"
#include "Window/WindowHandle.h"
#include "String/Path.h"

namespace tyr
{
	class Camera;

	struct WorldConfig
	{
		Name name;
		// The render window the world's scene will draw to. Must come from RendererAPI::AddWindow.
		// The world doesn't own it - whoever added the window removes it.
		RenderWindowHandle windowHandle;
		// The OS window backing windowHandle above. Lets WorldManager poll for an externally
		// (OS-)detected resize each tick and forward it to the renderer.
		WindowHandle osWindowHandle;
		ViewArea viewArea;
		// The world is provided the camera (will be a component later) but its dimensions will be updated by the world manager when the window resizes
		Camera* camera = nullptr;
		// Off for a world being edited in the level editor, so physics and audio don't run in it.
		bool simulate = true;
	};

	constexpr uint c_MaxActorInstanceEntities = 32;

	// An actor placed in a world, with the name and folder it's shown under in the editor.
	// Identify it by its root entity rather than its index, since indices change on removal.
	struct ActorInstance
	{
		Name name;
		// Folder in the level hierarchy, e.g. "Enemies/Boss".
		RelativePath folderPath;
		LocalArray<Entity, c_MaxActorInstanceEntities> entities;

		Entity RootEntity() const { return entities[0]; }
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
		// Physics and audio only run in simulated worlds.
		bool simulate = true;
		EntitySystem entities;
		// The entities version the renderer-side mesh instances and lights were last synced at.
		uint syncedEntitiesVersion = 0;
		Array<ActorInstance> actorInstances;

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
			simulate = true;
			entities.Reset();
			syncedEntitiesVersion = 0;
			actorInstances.Clear();
		}
	};
}
