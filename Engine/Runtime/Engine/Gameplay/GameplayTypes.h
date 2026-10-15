#pragma once

#include "Core.h"
#include "String/LocalString.h"
#include "Math/Vector3.h"
#include "ECS/EntitySystem.h"

namespace tyr
{
	// A sound something made that enemies can hear, out to its radius.
	struct NoiseEvent
	{
		Vector3 position;
		// Metres it carries.
		float radius = 0.0f;
		// Seconds since it was made.
		float age = 0.0f;
		// What made it, or c_InvalidEntity.
		Entity source = c_InvalidEntity;
	};
	constexpr uint c_MaxNoiseEvents = 64;

	// How a door moves between closed and open. Stored as uint8 in DoorComponent.
	enum class DoorMotion : uint8
	{
		// Turns about its Y axis, through the entity's origin, which is its hinge.
		Swing = 0,
		// Moves along its slide offset, such as a sliding or roller door.
		Slide
	};

	// What stops a door being opened by hand. Stored as uint8 in DoorComponent.
	enum class DoorLock : uint8
	{
		Unlocked = 0,
		// Opens once the user holds its key, which then unlocks it for good.
		NeedsKey,
		// Only unlocks from its front, the side its Z axis points to.
		LockedFromBack,
		// Only a door panel or gameplay can open it.
		Locked
	};

	// How a light fixture works. Stored as uint8 in LightFixtureComponent.
	enum class LightFixtureState : uint8
	{
		Working = 0,
		// Never lights.
		Broken,
		// Lights, but keeps dropping out.
		Flickering
	};

	// Names for keys and other items, such as "Red Keycard".
	constexpr uint c_MaxItemName = 31;
	using ItemName = LocalString<c_MaxItemName>;
	// Names the light fixtures and switches that go together, such as "Lobby".
	using CircuitName = LocalString<c_MaxItemName>;

	struct GameplayConstants
	{
		// How far from their eyes the player can use things, in metres.
		static constexpr float c_InteractDistance = 2.0f;
		static constexpr uint c_MaxInventoryItems = 12;
	};
}
