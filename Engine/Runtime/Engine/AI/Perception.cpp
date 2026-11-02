#include "Perception.h"
#include "ECS/Components.h"
#include "World/World.h"
#include "Physics/PhysicsComponentsManager.h"
#include "Navigation/NavmeshComponentsManager.h"
#include "Gameplay/LightLevel.h"
#include "Math/Math.h"
#include <algorithm>

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
		constexpr float c_TargetWaistHeight = 0.6f;
		// A hit this close to what's being looked at, or listened for, isn't counted as being in the way.
		constexpr float c_ClearDistance = 0.3f;
		// Each wall a noise comes through halves how far it carries, and each shut door quarters it,
		// counting this many halvings at most.
		constexpr uint c_MaxNoiseHalvings = 4;
		constexpr uint c_ShutDoorHalvings = 2;
		// How far past a wall's surface the next one is looked for, so the same one isn't counted twice.
		constexpr float c_WallStep = 0.4f;
		// Following a noise round through doorways: how high above the floor shut doors are looked
		// for, the most hits checked along each stretch, and how near the noise a door is counted
		// as the one that made it, so it doesn't muffle itself.
		constexpr float c_DoorCheckHeight = 1.0f;
		constexpr uint c_MaxPathHits = 4;
		constexpr float c_OwnDoorDistance = 1.0f;
		// A door counts as shut until it's this far open.
		constexpr float c_ShutDoorAmount = 0.5f;

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

		bool IsShutDoor(const World& world, const RayCastEntityHit& hit, const Vector3& noisePosition)
		{
			if (!world.entities.HasComponent<DoorComponent>(hit.entity) || (hit.position - noisePosition).Length() <= c_OwnDoorDistance)
			{
				return false;
			}
			const DoorComponent& door = world.entities.GetComponent<DoorComponent>(hit.entity);
			return !door.broken && door.openAmount < c_ShutDoorAmount;
		}

		// How many times the walls, doors and the like between the two points halve a noise, up to the most that count.
		uint CountHalvings(World& world, const PhysicsComponentsManager& physics, const Vector3& from, const Vector3& to, const Vector3& noisePosition)
		{
			uint count = 0;
			Vector3 start = from;
			for (uint hits = 0; hits < c_MaxNoiseHalvings && count < c_MaxNoiseHalvings; ++hits)
			{
				const Vector3 offset = to - start;
				const float distance = offset.Length();
				if (distance <= c_ClearDistance)
				{
					break;
				}
				const Vector3 direction = offset * (1.0f / distance);
				RayCastEntityHit hit;
				if (!physics.Raycast(world.physicsWorld, start, direction, distance - c_ClearDistance, hit))
				{
					break;
				}
				count += IsShutDoor(world, hit, noisePosition) ? c_ShutDoorHalvings : 1;
				start = hit.position + direction * c_WallStep;
			}
			return std::min(count, c_MaxNoiseHalvings);
		}

		// How many shut doors are between the two points, leaving out one at the noise itself.
		uint CountShutDoors(World& world, const PhysicsComponentsManager& physics, const Vector3& from, const Vector3& to, const Vector3& noisePosition)
		{
			uint doors = 0;
			Vector3 start = from;
			for (uint hits = 0; hits < c_MaxPathHits; ++hits)
			{
				const Vector3 offset = to - start;
				const float distance = offset.Length();
				if (distance <= c_ClearDistance)
				{
					break;
				}
				const Vector3 direction = offset * (1.0f / distance);
				RayCastEntityHit hit;
				if (!physics.Raycast(world.physicsWorld, start, direction, distance - c_ClearDistance, hit))
				{
					break;
				}
				doors += IsShutDoor(world, hit, noisePosition) ? 1 : 0;
				start = hit.position + direction * c_WallStep;
			}
			return doors;
		}

		// Whether a noise carrying reach metres gets to the listener's feet, either straight through
		// whatever's in the way, each wall or shut door halving it, or along the navmesh round through
		// doorways, where only shut doors on the way halve it. The path is only searched for when going
		// straight isn't enough.
		bool Hears(World& world, const NavmeshComponentsManager& navmesh, const PhysicsComponentsManager& physics, const Vector3& eye,
			const Vector3& feet, const Vector3& noisePosition, float reach)
		{
			const float distance = (noisePosition - feet).Length();
			if (distance > reach)
			{
				return false;
			}
			const uint halvings = CountHalvings(world, physics, eye, noisePosition + Vector3(0.0f, 0.3f, 0.0f), noisePosition);
			if (distance <= reach / static_cast<float>(1u << halvings))
			{
				return true;
			}

			NavPath path;
			if (!navmesh.FindPath(world, noisePosition, feet, true, path) || path.partial || path.truncated)
			{
				return false;
			}
			const Vector3 up(0.0f, c_DoorCheckHeight, 0.0f);
			float travelled = 0.0f;
			Vector3 previous = noisePosition;
			for (const Vector3& corner : path.points)
			{
				travelled += (corner - previous).Length();
				if (travelled > reach)
				{
					return false;
				}
				const uint doors = CountShutDoors(world, physics, previous + up, corner + up, noisePosition);
				reach /= static_cast<float>(1u << std::min(doors * c_ShutDoorHalvings, c_MaxNoiseHalvings));
				if (travelled > reach)
				{
					return false;
				}
				previous = corner;
			}
			return true;
		}

		float DistanceToSegment(const Vector3& point, const Vector3& start, const Vector3& end)
		{
			const Vector3 along = end - start;
			const float lengthSquared = along.Dot(along);
			const float t = lengthSquared > 0.0f ? std::clamp((point - start).Dot(along) / lengthSquared, 0.0f, 1.0f) : 0.0f;
			return (point - (start + along * t)).Length();
		}

		// How much of its sight range it sees the target at, from how lit the target is.
		float GetLitShare(World& world, const PhysicsComponentsManager& physics, const PerceptionComponent& perception,
			const PerceivableComponent& target, const Vector3& chest)
		{
			if (!perception.sightNeedsLight)
			{
				return 1.0f;
			}
			const float light = LightLevel::Get(world, physics, chest);
			const float share = std::clamp((light - perception.darkLight) / std::max(perception.fullLight - perception.darkLight, 0.001f), 0.0f, 1.0f);
			return std::max(share, target.carryingLight ? perception.carriedLightShare : 0.0f);
		}

		void Know(PerceptionComponent& perception, Entity target, const Vector3& position, Sense sense)
		{
			perception.targetKnown = true;
			perception.target = target;
			perception.targetPosition = position;
			perception.targetAge = 0.0f;
			perception.targetSense = static_cast<uint8>(sense);
		}

		// Finds what it hunts within range and the width in front of it, with nothing in the way.
		void SenseInFront(World& world, const PhysicsComponentsManager& physics, PerceptionComponent& perception, const Vector3& eye,
			const Vector3& facing, float range, float angle, bool useVisibility, Sense sense)
		{
			const float halfAngleCos = Math::Cos(angle * 0.5f * Math::c_DegToRad);
			world.entities.ForEach<PerceivableComponent>([&](Entity targetEntity, const PerceivableComponent& target)
			{
				if (target.hidden || !world.entities.HasComponent<TransformComponent>(targetEntity))
				{
					return;
				}
				const Vector3& targetPosition = world.entities.GetComponent<TransformComponent>(targetEntity).world.position;
				const Vector3 chest = targetPosition + Vector3(0.0f, c_TargetChestHeight, 0.0f);
				const Vector3 toTarget = chest - eye;
				const float distance = toTarget.Length();
				if (distance > range * (useVisibility ? target.visibility : 1.0f) || distance < 0.0001f)
				{
					return;
				}
				const Vector3 flat(toTarget.x, 0.0f, toTarget.z);
				const float flatLength = flat.Length();
				if (flatLength > 0.0001f && flat.Dot(facing) / flatLength < halfAngleCos)
				{
					return;
				}
				if (useVisibility && distance > range * target.visibility * GetLitShare(world, physics, perception, target, chest))
				{
					return;
				}
				if (Clear(world, physics, eye, chest))
				{
					Know(perception, targetEntity, targetPosition, sense);
				}
			});
		}
	}

	namespace Perception
	{
		void Update(World& world, const NavmeshComponentsManager& navmesh, const PhysicsComponentsManager& physics, float deltaTime)
		{
			world.entities.ForEach<PerceptionComponent>([&](Entity entity, PerceptionComponent& perception)
			{
				perception.targetAge += deltaTime;
				perception.updateTimer -= deltaTime;
				if (perception.updateTimer > 0.0f || !world.entities.HasComponent<TransformComponent>(entity))
				{
					return;
				}
				perception.updateTimer = c_UpdateInterval;

				const Transform& transform = world.entities.GetComponent<TransformComponent>(entity).world;
				const Vector3 eye = transform.position + Vector3(0.0f, c_EyeHeight, 0.0f);
				const Vector3 facing = transform.rotation.Rotate(Vector3::c_Forward);

				// Touch: whatever it hunts, close enough to brush against anywhere along its body.
				const Vector3 bodyEnd = transform.position + facing * perception.bodyLength;
				world.entities.ForEach<PerceivableComponent>([&](Entity targetEntity, const PerceivableComponent& target)
				{
					if (!target.hidden && world.entities.HasComponent<TransformComponent>(targetEntity))
					{
						const Vector3& targetPosition = world.entities.GetComponent<TransformComponent>(targetEntity).world.position;
						if (DistanceToSegment(targetPosition, transform.position, bodyEnd) <= perception.touchRange)
						{
							Know(perception, targetEntity, targetPosition, Sense::Touch);
						}
					}
				});

				// Sight, and sensing what's close in front. Being harder to see only shortens sight.
				if (perception.sight)
				{
					SenseInFront(world, physics, perception, eye, facing, perception.sightRange, perception.sightAngle, true, Sense::Sight);
				}
				if (perception.nearby)
				{
					SenseInFront(world, physics, perception, eye, facing, perception.nearbyRange, perception.nearbyAngle, false, Sense::Nearby);
				}

				// Once it's after its target, it keeps track of it while nothing's in the way between them,
				// to its chest or, crouched behind something low, its waist.
				// One that needs light to see loses track of it in the dark.
				const bool targetHidden = perception.target != c_InvalidEntity && world.entities.HasComponent<PerceivableComponent>(perception.target)
					&& world.entities.GetComponent<PerceivableComponent>(perception.target).hidden;
				if (perception.tracking && perception.targetKnown && perception.target != c_InvalidEntity && !targetHidden
					&& world.entities.HasComponent<TransformComponent>(perception.target))
				{
					const Vector3& targetPosition = world.entities.GetComponent<TransformComponent>(perception.target).world.position;
					const float distance = (targetPosition - transform.position).Length();
					const bool inRange = perception.trackRange > 0.0f && distance <= perception.trackRange
						&& (!perception.sightNeedsLight || distance <= perception.trackRange * GetLitShare(world, physics, perception,
							world.entities.GetComponent<PerceivableComponent>(perception.target), targetPosition + Vector3(0.0f, c_TargetChestHeight, 0.0f)));
					if (inRange && (Clear(world, physics, eye, targetPosition + Vector3(0.0f, c_TargetChestHeight, 0.0f))
						|| Clear(world, physics, eye, targetPosition + Vector3(0.0f, c_TargetWaistHeight, 0.0f))))
					{
						Know(perception, perception.target, targetPosition, Sense::Tracked);
					}
				}

				// Hearing: each new noise once, if it carries to its feet. Hearing what it hunts, such as its
				// footsteps, tells it where that is. What it does about any other noise is up to its brain.
				if (perception.hearing)
				{
					uint newestId = perception.lastNoiseId;
					for (const NoiseEvent& noise : world.noises)
					{
						if (noise.age > c_FreshNoise || noise.source == entity || noise.id <= perception.lastNoiseId)
						{
							continue;
						}
						newestId = std::max(newestId, noise.id);
						const bool fromTarget = noise.source != c_InvalidEntity && world.entities.HasComponent<PerceivableComponent>(noise.source);
						const Vector3 toNoise = noise.position - transform.position;
						if (fromTarget && noise.radius <= perception.quietBehind && Vector3(toNoise.x, 0.0f, toNoise.z).Dot(facing) < 0.0f)
						{
							continue;
						}
						if (!Hears(world, navmesh, physics, eye, transform.position, noise.position, noise.radius * perception.hearingScale))
						{
							continue;
						}
						// Its target hiding can only be heard working the hiding place's door, which gives away
						// that it's in there.
						if (fromTarget && world.entities.GetComponent<PerceivableComponent>(noise.source).hidden)
						{
							perception.heardHiddenTarget = noise.source;
						}
						else if (fromTarget)
						{
							Know(perception, noise.source, noise.position, Sense::Hearing);
						}
						// The loudest one heard wins.
						if (perception.heardNoise && noise.radius < perception.noiseRadius)
						{
							continue;
						}
						perception.heardNoise = true;
						perception.noiseRadius = noise.radius;
						perception.noisePosition = noise.position;
						perception.noiseSource = noise.source != c_InvalidEntity && world.entities.HasComponent<PerceivableComponent>(noise.source)
							? noise.source : c_InvalidEntity;
					}
					perception.lastNoiseId = newestId;
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
