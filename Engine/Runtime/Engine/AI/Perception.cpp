#include "Perception.h"
#include "ECS/Components.h"
#include "World/World.h"
#include "Physics/PhysicsComponentsManager.h"
#include "Math/Math.h"

namespace tyr
{
	namespace
	{
		// Senses are checked this often, in seconds, rather than every frame.
		constexpr float c_UpdateInterval = 0.1f;
		// Noises younger than this haven't been listened for yet.
		constexpr float c_FreshNoise = 0.2f;
		// Where an enemy senses from, and where on a target it looks, above their feet.
		constexpr float c_EyeHeight = 1.4f;
		constexpr float c_TargetChestHeight = 1.2f;
		// A hit this close to what's being looked at, or listened for, isn't counted as being in the way.
		constexpr float c_ClearDistance = 0.3f;

		bool Clear(World& world, const PhysicsComponentsManager& physics, const Vector3& from, const Vector3& to)
		{
			const Vector3 offset = to - from;
			const float distance = offset.Length();
			if (distance <= c_ClearDistance)
			{
				return true;
			}
			RayCastEntityHit hit;
			return !physics.Raycast(world.physicsWorld, from, offset * (1.0f / distance), distance - c_ClearDistance, hit);
		}

		void Know(PerceptionComponent& perception, Entity target, const Vector3& position, Sense sense)
		{
			perception.targetKnown = true;
			perception.target = target;
			perception.targetPosition = position;
			perception.targetAge = 0.0f;
			perception.targetSense = static_cast<uint8>(sense);
		}
	}

	namespace Perception
	{
		void Update(World& world, const PhysicsComponentsManager& physics, float deltaTime)
		{
			world.entities.ForEach<PerceptionComponent>([&](Entity entity, PerceptionComponent& perception)
			{
				perception.targetAge += deltaTime;
				perception.updateTimer -= deltaTime;
				if (perception.updateTimer > 0.0f || !world.entities.HasComponent<ComponentTransform>(entity))
				{
					return;
				}
				perception.updateTimer = c_UpdateInterval;

				const Transform& transform = world.entities.GetComponent<ComponentTransform>(entity).world;
				const Vector3 eye = transform.position + Vector3(0.0f, c_EyeHeight, 0.0f);
				const Vector3 facing = transform.rotation.Rotate(Vector3::c_Forward);

				// Touch: whatever it hunts, close enough to brush against.
				world.entities.ForEach<PerceivableComponent>([&](Entity targetEntity, const PerceivableComponent& /*target*/)
				{
					if (world.entities.HasComponent<ComponentTransform>(targetEntity))
					{
						const Vector3& targetPosition = world.entities.GetComponent<ComponentTransform>(targetEntity).world.position;
						if ((targetPosition - transform.position).Length() <= perception.touchRange)
						{
							Know(perception, targetEntity, targetPosition, Sense::Touch);
						}
					}
				});

				// Sight: within range and the width of its view, with nothing in the way.
				if (perception.sight)
				{
					const float halfAngleCos = Math::Cos(perception.sightAngle * 0.5f * Math::c_DegToRad);
					world.entities.ForEach<PerceivableComponent>([&](Entity targetEntity, const PerceivableComponent& target)
					{
						if (!world.entities.HasComponent<ComponentTransform>(targetEntity))
						{
							return;
						}
						const Vector3& targetPosition = world.entities.GetComponent<ComponentTransform>(targetEntity).world.position;
						const Vector3 chest = targetPosition + Vector3(0.0f, c_TargetChestHeight, 0.0f);
						const Vector3 toTarget = chest - eye;
						const float distance = toTarget.Length();
						if (distance > perception.sightRange * target.visibility || distance < 0.0001f)
						{
							return;
						}
						const Vector3 flat(toTarget.x, 0.0f, toTarget.z);
						const float flatLength = flat.Length();
						if (flatLength > 0.0001f && flat.Dot(facing) / flatLength < halfAngleCos)
						{
							return;
						}
						if (Clear(world, physics, eye, chest))
						{
							Know(perception, targetEntity, targetPosition, Sense::Sight);
						}
					});
				}

				// Hearing: new noises within how far they carry, which a wall in the way halves.
				if (perception.hearing)
				{
					for (const NoiseEvent& noise : world.noises)
					{
						if (noise.age > c_FreshNoise || noise.source == entity)
						{
							continue;
						}
						const float distance = (noise.position - eye).Length();
						float reach = noise.radius * perception.hearingScale;
						if (distance > reach)
						{
							continue;
						}
						if (!Clear(world, physics, eye, noise.position + Vector3(0.0f, 0.3f, 0.0f)))
						{
							reach *= 0.5f;
						}
						if (distance > reach)
						{
							continue;
						}
						if (noise.source != c_InvalidEntity && world.entities.HasComponent<PerceivableComponent>(noise.source))
						{
							Know(perception, noise.source, noise.position, Sense::Hearing);
						}
						else
						{
							perception.heardOther = true;
							perception.otherPosition = noise.position;
						}
					}
				}

				// Smell: the freshest scent of a target near its nose, which leads it along the trail.
				if (perception.smell)
				{
					const ScentMarker* freshest = nullptr;
					for (const ScentMarker& marker : world.scents)
					{
						if (marker.age > perception.smellMaxAge || (freshest && marker.age >= freshest->age)
							|| (marker.position - transform.position).Length() > perception.smellRange
							|| marker.source == c_InvalidEntity || !world.entities.HasComponent<PerceivableComponent>(marker.source))
						{
							continue;
						}
						freshest = &marker;
					}
					// What it can see or hear just now is better than a trail.
					const bool knowsBetter = perception.targetKnown && perception.targetAge < c_UpdateInterval * 2.0f
						&& perception.targetSense != static_cast<uint8>(Sense::Smell);
					// The same marker again isn't news, so at the end of a cold trail what it knows ages.
					const bool sameMarker = freshest && perception.targetKnown && perception.targetSense == static_cast<uint8>(Sense::Smell)
						&& (freshest->position - perception.targetPosition).Length() < 0.1f;
					if (freshest && !knowsBetter && !sameMarker)
					{
						Know(perception, freshest->source, freshest->position, Sense::Smell);
					}
				}
			});
		}
	}
}
