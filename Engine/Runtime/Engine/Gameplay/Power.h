#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "ECS/EntitySystem.h"
#include "Math/Vector3.h"

namespace tyr
{
	struct World;
	struct LightFixtureComponent;

	namespace Power
	{
		// Whether the world has power, which the game decides, such as from its generators. A world
		// being edited always has power, so its lighting shows.
		TYR_ENGINE_API bool IsOn(const World& world);
		TYR_ENGINE_API void SetOn(World& world, bool on);

		constexpr uint c_MaxZones = 32;
		// Whether there's power at the position: the world has it, and the power zone it's in, if any, is on.
		TYR_ENGINE_API bool IsOn(World& world, const Vector3& position);
		// The power zone the position is in, or -1 when it's in none.
		TYR_ENGINE_API int GetZone(World& world, const Vector3& position);
		// Switches a power zone on or off, such as from a switchboard. Zones start switched on.
		TYR_ENGINE_API bool IsZoneOn(const World& world, uint zone);
		TYR_ENGINE_API void SetZoneOn(World& world, uint zone, bool on);

		// Flips the light switch, and every light fixture on its circuit to match it.
		TYR_ENGINE_API void ToggleSwitch(World& world, Entity lightSwitch);

		// How bright the fixture is now, from 0 off to 1 fully on, moving its flicker on by deltaTime
		// when animate is set.
		TYR_ENGINE_API float UpdateFixture(LightFixtureComponent& fixture, bool powered, bool animate, float deltaTime);
	}
}
