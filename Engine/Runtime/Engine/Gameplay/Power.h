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
		// Works out whether the world has power: while one of its generators runs, or always when it
		// has none. A world being edited always has power, so its lighting shows.
		TYR_ENGINE_API void Update(World& world);

		// True when the world has power, as of its last update.
		TYR_ENGINE_API bool IsOn(const World& world);

		// Starts the generator, powering the world straight away.
		TYR_ENGINE_API void StartGenerator(World& world, Entity generator);

		// Flips the light switch, and every light fixture on its circuit to match it.
		TYR_ENGINE_API void ToggleSwitch(World& world, Entity lightSwitch);

		// How bright the fixture is now, from 0 off to 1 fully on, moving its flicker on by deltaTime
		// when animate is set.
		TYR_ENGINE_API float UpdateFixture(LightFixtureComponent& fixture, bool powered, bool animate, float deltaTime);
	}
}
