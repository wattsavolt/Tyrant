#pragma once

#include "Core.h"
#include "Math/Vector3.h"
#include "Math/Matrix4.h"
#include "EngineMacros.h"
#include "Rendering/Scene.h"
#include "AssetSystem/AssetID.h"

namespace tyr
{
	class Camera;

	struct WorldConfig
	{
		Name name;
		// TODO : Add window index later if required
		ViewArea viewArea;
		// The world is provided the camera (will be a component later) but its dimensions will be updated by the world manager when the window resizes
		Camera* camera = nullptr;
	};

	// A world / scene in an app. Just data - WorldManager owns all the logic that
	// creates, updates, and tears one of these down, and callers reach a world through
	// WorldManager via its handle rather than calling anything on World itself.
	struct World
	{
		Name name;
		Camera* camera = nullptr;
		ViewArea viewArea;
		uint sceneIndex = RenderConstants::c_MaxScenes;
		bool active = true;
		bool visible = true;

		// Puts a recycled pool slot back to a blank state before WorldManager::InitWorld
		// fills it in again for a new world. Once World has an Array (or similar)
		// member, this is where its capacity gets reused instead of freed.
		void Reset()
		{
			name = {};
			camera = nullptr;
			viewArea = {};
			sceneIndex = RenderConstants::c_MaxScenes;
			active = true;
			visible = true;
		}
	};
}
