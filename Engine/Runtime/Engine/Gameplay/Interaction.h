#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "Math/Vector3.h"
#include "ECS/EntitySystem.h"
#include "String/LocalString.h"

namespace tyr
{
	struct World;
	class WorldManager;
	class InputManager;

	using InteractionText = LocalString<63>;

	namespace Interaction
	{
		// The entity at or above the one hit that can be used, such as a door, panel or pickup.
		TYR_ENGINE_API Entity FindUsable(const World& world, Entity hit);

		// What using it would do, such as "Open" or "Take Red Keycard".
		TYR_ENGINE_API void Describe(const World& world, Entity target, InteractionText& outText);

		// Uses it on behalf of the user, whose inventory holds any keys. Returns false when
		// nothing happens, such as at a locked door, and outMessage says why.
		TYR_ENGINE_API bool Use(WorldManager& worldManager, Handle worldHandle, Entity target, Entity user, InteractionText& outMessage);
	}

	// Lets the player use whatever they look at within reach, with E or the gamepad's X button.
	// Shows what it would do, and why when it can't.
	class TYR_ENGINE_API InteractionController final
	{
	public:
		void Update(WorldManager& worldManager, Handle worldHandle, Entity user, const InputManager& input, float deltaTime, bool hasInput);

	private:
		InteractionText m_Message;
		Vector3 m_MessagePosition;
		float m_MessageTime = 0.0f;
	};
}
