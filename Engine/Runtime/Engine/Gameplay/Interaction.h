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

	// Lets a game add its own things to use, such as a hiding place, and take over the use button
	// while there's only one thing to do, such as get back out of one.
	class TYR_ENGINE_API InteractionHandler
	{
	public:
		virtual ~InteractionHandler() = default;

		// The game's thing to use for this entity, such as the hiding place whose door it is, or c_InvalidEntity.
		virtual Entity FindUsable(const World& world, Entity entity) const = 0;
		// What using it would do, such as "Hide". Left empty, nothing is shown.
		virtual void Describe(const World& world, Entity target, InteractionText& outAction) const = 0;
		// Returns false when nothing happens, and outMessage says why.
		virtual bool Use(World& world, Entity target, Entity user, InteractionText& outMessage) = 0;
		// While the user can only do one thing, says what, such as "Get out". False otherwise.
		virtual bool GetOnlyAction(const World& world, Entity user, InteractionText& outAction) const = 0;
		virtual void UseOnlyAction(World& world, Entity user) = 0;
	};

	namespace Interaction
	{
		// The game's own things to use, until it's set back to null. The game keeps it alive meanwhile.
		TYR_ENGINE_API void SetHandler(InteractionHandler* handler);

		// While the user can only do one thing, such as get out of hiding, does it and returns true.
		TYR_ENGINE_API bool UseOnlyAction(World& world, Entity user);

		// The entity at or above the one hit that can be used, such as a door, panel or pickup.
		TYR_ENGINE_API Entity FindUsable(const World& world, Entity hit);

		// What using it would do, such as "[E] Open" or "[E] Take Red Keycard", shown with the button that uses it.
		TYR_ENGINE_API void Describe(const World& world, Entity target, InteractionText& outText, const char* useButton = "E");

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
