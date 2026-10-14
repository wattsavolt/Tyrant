#pragma once

#include "Core.h"
#include "String/LocalString.h"

namespace tyr
{
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

	// Names for keys and other items, such as "Red Keycard".
	constexpr uint c_MaxItemName = 31;
	using ItemName = LocalString<c_MaxItemName>;

	struct GameplayConstants
	{
		// How far from their eyes the player can use things, in metres.
		static constexpr float c_InteractDistance = 2.0f;
		static constexpr uint c_MaxInventoryItems = 12;
	};
}
