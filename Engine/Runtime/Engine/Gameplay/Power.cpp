#include "Power.h"
#include "ECS/Components.h"
#include "World/World.h"

namespace tyr
{
	namespace
	{
		// How bright a flickering fixture is when it drops out, so it never goes fully dark.
		constexpr float c_FlickerLow = 0.3f;
	}

	namespace Power
	{
		bool IsOn(const World& world)
		{
			return !world.simulate || world.powered;
		}

		void SetOn(World& world, bool on)
		{
			world.powered = on;
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

			// Steadily lit for a few seconds, then a quick burst of a few dips, like a failing tube.
			fixture.flickerTimer -= deltaTime;
			if (fixture.flickerTimer <= 0.0f)
			{
				fixture.flickerSeed = fixture.flickerSeed * 1664525u + 1013904223u;
				const float random = static_cast<float>(fixture.flickerSeed >> 8) * (1.0f / 16777216.0f);
				fixture.flickerLit = !fixture.flickerLit;
				const bool burstGoesOn = random < 0.6f;
				fixture.flickerTimer = !fixture.flickerLit ? 0.04f + random * 0.08f
					: burstGoesOn ? 0.05f + random * 0.12f : 2.0f + (random - 0.6f) * 10.0f;
			}
			return fixture.flickerLit ? 1.0f : c_FlickerLow;
		}
	}
}
