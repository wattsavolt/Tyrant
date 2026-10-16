#include "NavAgents.h"
#include "ECS/Components.h"
#include "World/World.h"
#include "Navigation/NavmeshComponentsManager.h"
#include "Physics/PhysicsComponentsManager.h"
#include "Gameplay/Doors.h"
#include "Math/Math.h"
#include <algorithm>

namespace tyr
{
	namespace
	{
		// A path is found again this often while its target keeps moving, once it's moved this far.
		constexpr float c_RepathInterval = 0.5f;
		constexpr float c_RepathTargetMove = 0.5f;
		// A corner this close, across the floor, has been reached.
		constexpr float c_CornerReached = 0.25f;
		// It slows down over this last stretch to its target.
		constexpr float c_SlowDownDistance = 1.0f;
		// Moving less than this between checks counts as stalled. Stalling this many times in a row
		// means it's stuck, such as at a locked door.
		constexpr float c_StuckCheckInterval = 1.0f;
		constexpr float c_StuckMinProgress = 0.2f;
		constexpr uint c_MaxStalls = 3;
		// Doors are looked for this far ahead, at about waist height.
		constexpr float c_DoorCheckDistance = 1.0f;
		constexpr float c_DoorCheckHeight = 1.0f;

		float FlatDistance(const Vector3& a, const Vector3& b)
		{
			const float x = a.x - b.x;
			const float z = a.z - b.z;
			return Math::Sqrt(x * x + z * z);
		}

		Vector3 MoveTowards(const Vector3& current, const Vector3& target, float maxDistance)
		{
			const Vector3 toTarget = target - current;
			const float distance = toTarget.Length();
			return distance <= maxDistance || distance == 0.0f ? target : current + toTarget * (maxDistance / distance);
		}

		// Turns from current towards target by at most maxStep, the short way round. Radians.
		float TurnTowards(float current, float target, float maxStep)
		{
			float difference = target - current;
			while (difference > Math::c_Pi)
			{
				difference -= Math::c_TwoPi;
			}
			while (difference < -Math::c_Pi)
			{
				difference += Math::c_TwoPi;
			}
			return current + Math::Clamp(difference, -maxStep, maxStep);
		}

		// Opens an unlocked door just ahead, the way a person would on reaching it.
		void OpenDoorAhead(World& world, const PhysicsComponentsManager& physics, const Vector3& position, const Vector3& direction)
		{
			RayCastEntityHit hit;
			const Vector3 eye = position + Vector3(0.0f, c_DoorCheckHeight, 0.0f);
			if (!physics.Raycast(world.physicsWorld, eye, direction, c_DoorCheckDistance, hit) || !world.entities.HasComponent<DoorComponent>(hit.entity))
			{
				return;
			}
			const DoorComponent& door = world.entities.GetComponent<DoorComponent>(hit.entity);
			const DoorLock lock = static_cast<DoorLock>(door.lock);
			const bool canOpen = lock == DoorLock::Unlocked || (lock == DoorLock::LockedFromBack && Doors::IsInFront(world, hit.entity, position));
			if (canOpen && !Doors::IsOpening(world, hit.entity))
			{
				Doors::SetOpen(world, hit.entity, true, position);
			}
		}

		// It's done walking: there, or as close as it can get.
		void Finish(NavAgentComponent& agent, bool reached)
		{
			agent.hasTarget = false;
			agent.arrived = reached;
			agent.stuck = !reached;
			agent.path.points.Clear();
			agent.nextCorner = 0;
		}
	}

	namespace NavAgents
	{
		void SetTarget(World& world, Entity entity, const Vector3& target)
		{
			if (!world.entities.HasComponent<NavAgentComponent>(entity))
			{
				return;
			}
			NavAgentComponent& agent = world.entities.GetComponent<NavAgentComponent>(entity);
			agent.target = target;
			agent.hasTarget = true;
			agent.arrived = false;
			agent.stuck = false;
			// Found again on the next update.
			agent.path.points.Clear();
			agent.nextCorner = 0;
			agent.stuckTimer = 0.0f;
			agent.stuckCount = 0;
			if (world.entities.HasComponent<ComponentTransform>(entity))
			{
				agent.stuckCheckPosition = world.entities.GetComponent<ComponentTransform>(entity).world.position;
			}
		}

		void Stop(World& world, Entity entity)
		{
			if (world.entities.HasComponent<NavAgentComponent>(entity))
			{
				NavAgentComponent& agent = world.entities.GetComponent<NavAgentComponent>(entity);
				agent.hasTarget = false;
				agent.path.points.Clear();
				agent.nextCorner = 0;
			}
		}

		void Update(World& world, const NavmeshComponentsManager& navmesh, const PhysicsComponentsManager& physics, float deltaTime)
		{
			world.entities.ForEach<NavAgentComponent>([&](Entity entity, NavAgentComponent& agent)
			{
				if (!world.entities.HasComponent<CharacterComponent>(entity) || !world.entities.HasComponent<ComponentTransform>(entity))
				{
					return;
				}
				CharacterComponent& character = world.entities.GetComponent<CharacterComponent>(entity);
				const Transform& transform = world.entities.GetComponent<ComponentTransform>(entity).world;
				const Vector3 position = transform.position;
				if (!agent.yawKnown)
				{
					const Vector3 facing = transform.rotation.Rotate(Vector3::c_Forward);
					agent.yaw = Math::Atan2(facing.x, facing.z);
					agent.yawKnown = true;
				}

				Vector3 desired = Vector3::c_Zero;
				if (agent.hasTarget)
				{
					// A new path when there's none yet, or now and then while the target moves.
					agent.repathTimer -= deltaTime;
					const bool targetMoved = agent.repathTimer <= 0.0f && FlatDistance(agent.target, agent.pathTarget) > c_RepathTargetMove;
					if (agent.path.points.IsEmpty() || targetMoved)
					{
						agent.repathTimer = c_RepathInterval;
						agent.pathTarget = agent.target;
						agent.nextCorner = 1;
						if (!navmesh.FindPath(world, position, agent.target, agent.path))
						{
							Finish(agent, false);
						}
					}
				}

				if (agent.hasTarget)
				{
					const uint lastCorner = agent.path.points.Size() - 1;
					while (agent.nextCorner < lastCorner && FlatDistance(position, agent.path.points[agent.nextCorner]) < c_CornerReached)
					{
						++agent.nextCorner;
					}

					const Vector3& end = agent.path.points[lastCorner];
					const float toEnd = FlatDistance(position, end);
					const bool onLastLeg = agent.nextCorner >= lastCorner;
					if (onLastLeg && toEnd <= agent.arriveDistance && agent.path.truncated)
					{
						// Only the first part of a long path is kept, so the rest is found from here.
						agent.path.points.Clear();
					}
					else if (onLastLeg && toEnd <= agent.arriveDistance)
					{
						// A path that stopped short ends as close as it can get, which isn't there.
						Finish(agent, !agent.path.partial);
					}
					else
					{
						const Vector3& corner = agent.path.points[std::min(agent.nextCorner, lastCorner)];
						Vector3 direction(corner.x - position.x, 0.0f, corner.z - position.z);
						const float length = direction.Length();
						if (length > 0.0001f)
						{
							direction = direction * (1.0f / length);
							const float speed = onLastLeg ? agent.speed * std::min(1.0f, toEnd / c_SlowDownDistance) : agent.speed;
							desired = direction * speed;
							OpenDoorAhead(world, physics, position, direction);
						}
					}

					// Barely moving for a while means something's in the way: try a new path, then give up.
					agent.stuckTimer += deltaTime;
					if (agent.hasTarget && agent.stuckTimer >= c_StuckCheckInterval)
					{
						const bool stalled = FlatDistance(position, agent.stuckCheckPosition) < c_StuckMinProgress;
						agent.stuckCount = stalled ? agent.stuckCount + 1 : 0;
						agent.stuckTimer = 0.0f;
						agent.stuckCheckPosition = position;
						if (agent.stuckCount >= c_MaxStalls)
						{
							Finish(agent, false);
							desired = Vector3::c_Zero;
						}
						else if (stalled)
						{
							agent.path.points.Clear();
						}
					}
				}

				agent.velocity = MoveTowards(agent.velocity, desired, agent.acceleration * deltaTime);
				character.moveVelocity = agent.velocity;

				// Faces the way it's walking.
				const float speedSquared = agent.velocity.x * agent.velocity.x + agent.velocity.z * agent.velocity.z;
				if (speedSquared > 0.01f)
				{
					const float targetYaw = Math::Atan2(agent.velocity.x, agent.velocity.z);
					agent.yaw = TurnTowards(agent.yaw, targetYaw, agent.turnSpeed * Math::c_DegToRad * deltaTime);
				}
			});
		}
	}
}
