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

	// A trace of scent something left behind, which enemies that smell can follow.
	struct ScentMarker
	{
		Vector3 position;
		// Seconds since it was left, and how long it lasts.
		float age = 0.0f;
		float lifetime = 0.0f;
		Entity source = c_InvalidEntity;
	};
	constexpr uint c_MaxScentMarkers = 256;

	// How an enemy last found out where its target is. Stored as uint8 in PerceptionComponent.
	enum class Sense : uint8
	{
		None = 0,
		Sight,
		Hearing,
		Smell,
		// Close enough to brush against.
		Touch
	};

	// What an enemy is doing. Stored as uint8 in EnemyComponent.
	enum class EnemyState : uint8
	{
		// Wandering about near where it started, pausing now and then.
		Wander = 0,
		// Going to look at a noise that didn't come from its target.
		Investigate,
		// Going after its target, or where it last knew the target was.
		Chase,
		// Close enough to hit its target, and hitting it.
		Attack,
		// Looking around where it lost its target.
		Search,
		// Held still by light.
		Dazed
	};

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

	// A sound event's path, such as "event:/Doors/Open". Empty plays nothing.
	using SoundName = LocalString<47>;

	// A sound asked for this frame, played once at a position.
	struct SoundRequest
	{
		SoundName sound;
		Vector3 position;
		float volume = 1.0f;
	};
	constexpr uint c_MaxSoundRequests = 64;

	struct GameplayConstants
	{
		// How far from their eyes the player can use things, in metres.
		static constexpr float c_InteractDistance = 2.0f;
		static constexpr uint c_MaxInventoryItems = 12;
	};
}
