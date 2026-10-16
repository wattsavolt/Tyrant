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
#include "Level/LevelSettings.h"
#include "Actor/ActorTypes.h"
#include "Gameplay/GameplayTypes.h"
#include "Rendering/PostProcessSettings.h"

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
		// Shows the world on the whole of its window, rather than in an editor panel.
		bool presentToWindow = false;
	};

	constexpr uint c_MaxActorInstanceEntities = 32;

	// An actor placed in a world, with the name and folder it's shown under in the editor.
	// Identify it by its root entity rather than its index, since indices change on removal.
	struct ActorInstance
	{
		// Made from a GUID when the actor is placed and kept for good, unlike its name.
		Id64 id;
		// The actor type it was made from.
		Id64 typeID;
		ActorName name;
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
		RenderWindowHandle renderWindowHandle;
		WindowHandle windowHandle;
		SceneHandle sceneHandle;
		// Created/destroyed alongside sceneHandle - the world's scene owns this for its whole
		// lifetime, regardless of whether this world is currently the active one.
		RenderViewportHandle renderViewportHandle;
		// Simulated worlds only.
		Handle physicsWorld;
		bool visible = true;
		// Physics and audio only run in simulated worlds.
		bool simulate = true;
		// Holds physics and animation still, such as while the editor has the game paused.
		bool paused = false;
		// Whether its lights and door panels have power, worked out each update.
		bool powered = true;
		EntitySystem entities;
		// The entities version the renderer-side mesh instances and lights were last synced at.
		uint syncedEntitiesVersion = 0;
		// Set while a decal is waiting for its material to load.
		bool decalsPending = false;
		// When set, the camera follows this entity, offset from its position, once physics has
		// moved it each frame. Gameplay still turns the camera.
		Entity cameraEntity = c_InvalidEntity;
		Vector3 cameraOffset;
		Array<ActorInstance> actorInstances;
		// Hierarchy folders, including ones with nothing in them.
		Array<RelativePath> folders;
		LevelSettings settings;
		// The sky texture loaded for settings, kept until settings names another.
		AssetID loadedSkyTexture;
		// Goes up with every edit to the level, so the editor can tell when there are unsaved changes.
		uint changeCount = 0;
		// The level's navmesh as it's saved, and the one loaded from it. The data is empty without one.
		Array<uint8> navmeshData;
		Handle navmesh;
		// Set when the data's been replaced, so the navmesh is loaded from it again.
		bool navmeshChanged = false;
		// Recent noises, for anything listening, and scent left behind, for anything smelling.
		LocalArray<NoiseEvent, c_MaxNoiseEvents> noises;
		LocalArray<ScentMarker, c_MaxScentMarkers> scents;
		// Sounds asked for this frame, played and cleared at the end of it.
		LocalArray<SoundRequest, c_MaxSoundRequests> sounds;
		// Set by gameplay, such as fading out or flashing red when the player is hurt.
		ScreenEffects screenEffects;

		// Puts a recycled pool slot back to a blank state before it's filled in again for a
		// new world. entities.Reset() drops every entity/archetype from whatever level was
		// previously using this slot.
		void Reset()
		{
			folders.Clear();
			settings = {};
			loadedSkyTexture = {};
			changeCount = 0;
			name = {};
			camera = nullptr;
			viewArea = {};
			renderWindowHandle = {};
			windowHandle = {};
			sceneHandle = {};
			renderViewportHandle = {};
			physicsWorld = {};
			visible = true;
			simulate = true;
			paused = false;
			entities.Reset();
			syncedEntitiesVersion = 0;
			decalsPending = false;
			cameraEntity = c_InvalidEntity;
			cameraOffset = Vector3::c_Zero;
			actorInstances.Clear();
			noises.Clear();
			scents.Clear();
			sounds.Clear();
			screenEffects = {};
			navmeshData.Clear();
			navmesh = {};
			navmeshChanged = false;
		}
	};
}
