#include "LightLevel.h"
#include "ECS/Components.h"
#include "World/World.h"
#include "Physics/PhysicsComponentsManager.h"
#include "Math/Math.h"
#include <algorithm>
#include <cmath>

namespace tyr
{
	namespace
	{
		// Light this faint isn't worth checking for walls in the way.
		constexpr float c_MinLight = 0.001f;
		// A directional light, like the moon, is checked for this far towards it.
		constexpr float c_DirectionalReach = 200.0f;
		// A hit this close to the light, or the position, isn't counted as being in the way.
		constexpr float c_ClearDistance = 0.15f;

		// 1 near the light, easing to exactly 0 at its range, the same as the renderer.
		float RangeWindow(float distance, float range)
		{
			const float ratio = distance / std::max(range, 0.0001f);
			const float ratio2 = ratio * ratio;
			const float window = std::clamp(1.0f - ratio2 * ratio2, 0.0f, 1.0f);
			return window * window;
		}

		// How bright the light is now, as its fixture or flashlight has it.
		float GetBrightness(const EntitySystem& entities, Entity entity)
		{
			if (entities.HasComponent<FlashlightComponent>(entity))
			{
				return entities.GetComponent<FlashlightComponent>(entity).on ? 1.0f : 0.0f;
			}
			if (entities.HasComponent<LightFixtureComponent>(entity))
			{
				const float brightness = entities.GetComponent<LightFixtureComponent>(entity).brightness;
				return brightness < 0.0f ? 1.0f : brightness;
			}
			return 1.0f;
		}

		float Attenuate(const Vector3& attenuation, float distance)
		{
			return std::max(attenuation.x + attenuation.y * distance + attenuation.z * distance * distance, 0.0001f);
		}

		float Strongest(const Vector3& colour)
		{
			return std::max(colour.x, std::max(colour.y, colour.z));
		}

		// Nothing but the light itself between the two points.
		bool Clear(World& world, const PhysicsComponentsManager& physics, const Vector3& from, const Vector3& to)
		{
			const Vector3 offset = to - from;
			const float distance = offset.Length();
			if (distance <= c_ClearDistance * 2.0f)
			{
				return true;
			}
			RayCastEntityHit hit;
			return !physics.Raycast(world.physicsWorld, from, offset * (1.0f / distance), distance - c_ClearDistance, hit);
		}
	}

	namespace LightLevel
	{
		float Get(World& world, const PhysicsComponentsManager& physics, const Vector3& position)
		{
			float total = 0.0f;
			world.entities.ForEach<PointLightComponent>([&](Entity entity, const PointLightComponent& light)
			{
				if (!world.entities.HasComponent<ComponentTransform>(entity))
				{
					return;
				}
				const Vector3& lightPosition = world.entities.GetComponent<ComponentTransform>(entity).world.position;
				const float distance = (lightPosition - position).Length();
				const float amount = light.intensity * GetBrightness(world.entities, entity) * RangeWindow(distance, light.range)
					* Strongest(light.colour) / Attenuate(light.attenuation, distance);
				if (amount > c_MinLight && Clear(world, physics, position, lightPosition))
				{
					total += amount;
				}
			});

			world.entities.ForEach<SpotLightComponent>([&](Entity entity, const SpotLightComponent& light)
			{
				if (!world.entities.HasComponent<ComponentTransform>(entity))
				{
					return;
				}
				const Transform& transform = world.entities.GetComponent<ComponentTransform>(entity).world;
				const Vector3 toPosition = position - transform.position;
				const float distance = toPosition.Length();
				const Vector3 direction = transform.rotation.Rotate(Vector3::c_Forward);
				const float alignment = distance > 0.0001f ? std::max(toPosition.Dot(direction) / distance, 0.0f) : 1.0f;
				const float amount = light.intensity * GetBrightness(world.entities, entity) * RangeWindow(distance, light.range)
					* std::pow(alignment, light.coneFalloff) * Strongest(light.colour) / Attenuate(light.attenuation, distance);
				if (amount > c_MinLight && Clear(world, physics, position, transform.position))
				{
					total += amount;
				}
			});

			world.entities.ForEach<DirLightComponent>([&](Entity /*entity*/, const DirLightComponent& light)
			{
				const float amount = light.intensity * Strongest(light.colour);
				const Vector3 towards = Vector3::Normalize(light.direction) * -c_DirectionalReach;
				if (amount > c_MinLight && Clear(world, physics, position, position + towards))
				{
					total += amount;
				}
			});
			return total;
		}

		bool IsInFlashlightBeam(World& world, const PhysicsComponentsManager& physics, const Vector3& position)
		{
			bool lit = false;
			world.entities.ForEach<FlashlightComponent>([&](Entity entity, const FlashlightComponent& flashlight)
			{
				if (lit || !flashlight.on || !flashlight.held || !world.entities.HasComponent<ComponentTransform>(entity))
				{
					return;
				}
				const Transform& transform = world.entities.GetComponent<ComponentTransform>(entity).world;
				const Vector3 toPosition = position - transform.position;
				const float distance = toPosition.Length();
				if (distance > flashlight.beamRange || distance < 0.0001f)
				{
					return;
				}
				const Vector3 direction = transform.rotation.Rotate(Vector3::c_Forward);
				if (toPosition.Dot(direction) / distance < Math::Cos(flashlight.beamAngle * Math::c_DegToRad))
				{
					return;
				}
				lit = Clear(world, physics, transform.position, position);
			});
			return lit;
		}
	}
}
