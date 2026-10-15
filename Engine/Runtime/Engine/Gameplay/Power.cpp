#include "Power.h"
#include "ECS/Components.h"
#include "World/World.h"

namespace tyr
{
	namespace
	{
		// How bright a flickering fixture is when it drops out, so it never goes fully dark.
		constexpr float c_FlickerLow = 0.08f;
	}

	namespace Power
	{
		void Update(World& world)
		{
			bool hasGenerator = false;
			bool running = false;
			world.entities.ForEach<GeneratorComponent>([&hasGenerator, &running](Entity /*entity*/, GeneratorComponent& generator)
			{
				hasGenerator = true;
				running = running || generator.running;
			});
			world.powered = !world.simulate || !hasGenerator || running;
		}

		bool IsOn(const World& world)
		{
			return world.powered;
		}

		void StartGenerator(World& world, Entity generator)
		{
			world.entities.GetComponent<GeneratorComponent>(generator).running = true;
			world.powered = true;
		}

		void ToggleSwitch(World& world, Entity lightSwitch)
		{
			LightSwitchComponent& switchComponent = world.entities.GetComponent<LightSwitchComponent>(lightSwitch);
			switchComponent.switchedOn = !switchComponent.switchedOn;

			const LightSwitchComponent& flipped = switchComponent;
			world.entities.ForEach<LightFixtureComponent>([&flipped](Entity /*entity*/, LightFixtureComponent& fixture)
			{
				if (fixture.circuit == flipped.circuit)
				{
					fixture.switchedOn = flipped.switchedOn;
				}
			});
			world.entities.ForEach<LightSwitchComponent>([&flipped](Entity /*entity*/, LightSwitchComponent& other)
			{
				if (other.circuit == flipped.circuit)
				{
					other.switchedOn = flipped.switchedOn;
				}
			});
		}

		float UpdateFixture(LightFixtureComponent& fixture, bool powered, bool animate, float deltaTime)
		{
			const LightFixtureState state = static_cast<LightFixtureState>(fixture.state);
			if (!powered || !fixture.switchedOn || state == LightFixtureState::Broken)
			{
				return 0.0f;
			}
			if (state != LightFixtureState::Flickering || !animate)
			{
				return 1.0f;
			}

			// Mostly lit, with short drop-outs that sometimes come a few at a time.
			fixture.flickerTimer -= deltaTime;
			if (fixture.flickerTimer <= 0.0f)
			{
				fixture.flickerSeed = fixture.flickerSeed * 1664525u + 1013904223u;
				const float random = static_cast<float>(fixture.flickerSeed >> 8) * (1.0f / 16777216.0f);
				fixture.flickerLit = !fixture.flickerLit;
				fixture.flickerTimer = fixture.flickerLit ? 0.04f + random * random * 3.0f : 0.03f + random * 0.15f;
			}
			return fixture.flickerLit ? 1.0f : c_FlickerLow;
		}
	}
}
