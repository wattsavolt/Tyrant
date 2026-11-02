#include "Power.h"
#include "ECS/Components.h"
#include "World/World.h"
#include "Math/Math.h"

namespace tyr
{
	namespace
	{
		// How bright a flickering fixture is when it drops out, so it never goes fully dark.
		constexpr float c_FlickerLow = 0.3f;
		// How bright a sparking fixture flashes, for how long, and the seconds between flashes.
		constexpr float c_SparkFlash = 0.6f;
		constexpr float c_SparkFlashMin = 0.05f;
		constexpr float c_SparkFlashMax = 0.12f;
		constexpr float c_SparkGapMin = 1.5f;
		constexpr float c_SparkGapMax = 7.0f;

		float NextRandom(LightFixtureComponent& fixture)
		{
			fixture.flickerSeed = fixture.flickerSeed * 1664525u + 1013904223u;
			return static_cast<float>(fixture.flickerSeed >> 8) * (1.0f / 16777216.0f);
		}
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

		int GetZone(World& world, const Vector3& position)
		{
			int zone = -1;
			float smallest = 0.0f;
			world.entities.ForEach<PowerZoneComponent>([&](Entity entity, const PowerZoneComponent& box)
			{
				if (box.zone >= c_MaxZones || !world.entities.HasComponent<TransformComponent>(entity))
				{
					return;
				}
				const Transform& transform = world.entities.GetComponent<TransformComponent>(entity).world;
				const Vector3 local = transform.rotation.Inverse().Rotate(position - transform.position);
				const Vector3 extents(box.halfExtents.x * transform.scale.x, box.halfExtents.y * transform.scale.y, box.halfExtents.z * transform.scale.z);
				const float size = extents.x * extents.y * extents.z;
				if (Math::Abs(local.x) <= extents.x && Math::Abs(local.y) <= extents.y && Math::Abs(local.z) <= extents.z
					&& (zone < 0 || size < smallest))
				{
					zone = box.zone;
					smallest = size;
				}
			});
			return zone;
		}

		bool IsZoneOn(const World& world, uint zone)
		{
			return !world.simulate || zone >= c_MaxZones || (world.unpoweredZones & (1u << zone)) == 0;
		}

		void SetZoneOn(World& world, uint zone, bool on)
		{
			if (zone < c_MaxZones)
			{
				world.unpoweredZones = on ? world.unpoweredZones & ~(1u << zone) : world.unpoweredZones | (1u << zone);
			}
		}

		bool IsOn(World& world, const Vector3& position)
		{
			if (!IsOn(world))
			{
				return false;
			}
			const int zone = GetZone(world, position);
			return zone < 0 || IsZoneOn(world, static_cast<uint>(zone));
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
			if (state == LightFixtureState::Sparking)
			{
				if (!animate)
				{
					return 0.0f;
				}
				// Dark for a few seconds, then a short flash as it sparks.
				fixture.flickerTimer -= deltaTime;
				if (fixture.flickerTimer <= 0.0f)
				{
					const float random = NextRandom(fixture);
					fixture.flickerLit = !fixture.flickerLit;
					fixture.sparked = fixture.flickerLit;
					fixture.flickerTimer = fixture.flickerLit ? c_SparkFlashMin + random * (c_SparkFlashMax - c_SparkFlashMin)
						: c_SparkGapMin + random * (c_SparkGapMax - c_SparkGapMin);
				}
				return fixture.flickerLit ? c_SparkFlash : 0.0f;
			}
			if (state != LightFixtureState::Flickering || !animate)
			{
				return 1.0f;
			}

			// Steadily lit for a few seconds, then a quick burst of a few dips, like a failing tube.
			fixture.flickerTimer -= deltaTime;
			if (fixture.flickerTimer <= 0.0f)
			{
				const float random = NextRandom(fixture);
				fixture.flickerLit = !fixture.flickerLit;
				const bool burstGoesOn = random < 0.6f;
				fixture.flickerTimer = !fixture.flickerLit ? 0.04f + random * 0.08f
					: burstGoesOn ? 0.05f + random * 0.12f : 2.0f + (random - 0.6f) * 10.0f;
			}
			return fixture.flickerLit ? 1.0f : c_FlickerLow;
		}
	}
}
