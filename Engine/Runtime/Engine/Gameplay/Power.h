#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "ECS/EntitySystem.h"

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

		// Flips the light switch, and every light fixture on its circuit to match it.
		TYR_ENGINE_API void ToggleSwitch(World& world, Entity lightSwitch);

		// How bright the fixture is now, from 0 off to 1 fully on, moving its flicker on by deltaTime
		// when animate is set.
		TYR_ENGINE_API float UpdateFixture(LightFixtureComponent& fixture, bool powered, bool animate, float deltaTime);
	}
}
