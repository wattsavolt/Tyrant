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
		// Doors are looked for at least this far ahead, at about waist height.
		constexpr float c_DoorCheckDistance = 1.3f;
		constexpr float c_DoorCheckHeight = 1.0f;
		// A door opening ahead is waited for until it's this far open, so it isn't walked into.
		constexpr float c_PassableOpenAmount = 0.75f;

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

		// Opens an unlocked door just ahead, the way a person would on reaching it, or bashes on it
		// until it bursts open, then waits for it to swing out of the way. True while it's held up.
		bool OpenDoorAhead(World& world, const PhysicsComponentsManager& physics, Entity entity, NavAgentComponent& agent, const Vector3& position,
			const Vector3& direction, float deltaTime)
		{
			// It stops at the door from as far out as it takes to slow down from how fast it's going.
			const float speedSquared = agent.velocity.x * agent.velocity.x + agent.velocity.z * agent.velocity.z;
			const float reach = agent.doorBashReach + speedSquared / (2.0f * std::max(agent.acceleration, 0.1f));
			RayCastEntityHit hit;
			const Vector3 eye = position + Vector3(0.0f, c_DoorCheckHeight, 0.0f);
			if (!physics.Raycast(world.physicsWorld, eye, direction, std::max(c_DoorCheckDistance, reach), hit) || !world.entities.HasComponent<DoorComponent>(hit.entity))
			{
				agent.bashDoor = c_InvalidEntity;
				return false;
			}
			const DoorComponent& door = world.entities.GetComponent<DoorComponent>(hit.entity);
			if (Doors::IsOpening(world, hit.entity))
			{
				agent.bashDoor = c_InvalidEntity;
				return door.openAmount < c_PassableOpenAmount && hit.distance <= reach;
			}
			const DoorLock lock = static_cast<DoorLock>(door.lock);
			if (lock != DoorLock::Unlocked && !(lock == DoorLock::LockedFromBack && Doors::IsInFront(world, hit.entity, position)))
			{
				agent.bashDoor = c_InvalidEntity;
				return false;
			}
			// It keeps coming until it's within reach of the door, or its blows will land on it.
			if (agent.bashDoor != hit.entity && hit.distance > reach)
			{
				return false;
			}
			if (agent.doorBashes <= 0)
			{
				Doors::SetOpen(world, hit.entity, true, position, entity);
				return true;
			}

			if (agent.bashDoor != hit.entity)
			{
				agent.bashDoor = hit.entity;
				agent.bashCount = 0;
				agent.bashTimer = 0.0f;
				agent.bashHitTimer = -1.0f;
			}
			// Each swing lands a moment after it starts, so the blow matches the animation.
			agent.bashTimer -= deltaTime;
			if (agent.bashTimer <= 0.0f)
			{
				agent.bashTimer = agent.doorBashInterval;
				agent.bashHitTimer = agent.doorBashHitDelay;
				agent.bashSwing++;
			}
			if (agent.bashHitTimer >= 0.0f)
			{
				agent.bashHitTimer -= deltaTime;
				if (agent.bashHitTimer < 0.0f && ++agent.bashCount >= agent.doorBashes)
				{
					Doors::BurstOpen(world, hit.entity, position, entity);
					agent.bashDoor = c_InvalidEntity;
					return true;
				}
				if (agent.bashHitTimer < 0.0f)
				{
					Doors::Bash(world, hit.entity, position, entity);
				}
			}
			return true;
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
			agent.hasFacePoint = false;
			agent.stuckCount = 0;
			if (world.entities.HasComponent<TransformComponent>(entity))
			{
				agent.stuckCheckPosition = world.entities.GetComponent<TransformComponent>(entity).world.position;
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
				agent.dashTimer = 0.0f;
			}
		}

		void Dash(World& world, Entity entity, const Vector3& velocity, float seconds)
		{
			if (world.entities.HasComponent<NavAgentComponent>(entity))
			{
				Stop(world, entity);
				NavAgentComponent& agent = world.entities.GetComponent<NavAgentComponent>(entity);
				agent.dashVelocity = Vector3(velocity.x, 0.0f, velocity.z);
				agent.dashTimer = seconds;
				agent.hasFacePoint = false;
			}
		}

		void FaceTowards(World& world, Entity entity, const Vector3& point)
		{
			if (world.entities.HasComponent<NavAgentComponent>(entity))
			{
				NavAgentComponent& agent = world.entities.GetComponent<NavAgentComponent>(entity);
				agent.facePoint = point;
				agent.hasFacePoint = true;
			}
		}

		void Update(World& world, const NavmeshComponentsManager& navmesh, const PhysicsComponentsManager& physics, float deltaTime)
		{
			world.entities.ForEach<NavAgentComponent>([&](Entity entity, NavAgentComponent& agent)
			{
				if (!world.entities.HasComponent<CharacterComponent>(entity) || !world.entities.HasComponent<TransformComponent>(entity))
				{
					return;
				}
				CharacterComponent& character = world.entities.GetComponent<CharacterComponent>(entity);
				const Transform& transform = world.entities.GetComponent<TransformComponent>(entity).world;
				const Vector3 position = transform.position;
				if (!agent.yawKnown)
				{
					const Vector3 facing = transform.rotation.Rotate(Vector3::c_Forward);
					agent.yaw = Math::Atan2(facing.x, facing.z);
					agent.yawKnown = true;
				}

				const float maxTurn = agent.turnSpeed * Math::c_DegToRad * deltaTime;
				if (agent.dashTimer > 0.0f)
				{
					agent.dashTimer -= deltaTime;
					agent.velocity = agent.dashTimer > 0.0f ? agent.dashVelocity : Vector3::c_Zero;
					character.moveVelocity = agent.velocity;
					if (agent.dashVelocity.x * agent.dashVelocity.x + agent.dashVelocity.z * agent.dashVelocity.z > 0.01f)
					{
						agent.yaw = TurnTowards(agent.yaw, Math::Atan2(agent.dashVelocity.x, agent.dashVelocity.z), maxTurn);
					}
					return;
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
						if (!navmesh.FindPath(world, position, agent.target, agent.usesStairs, agent.path))
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
							if (OpenDoorAhead(world, physics, entity, agent, position, direction, deltaTime))
							{
								// It stands at the door, facing it, and isn't stuck while it's bashing.
								desired = Vector3::c_Zero;
								agent.facePoint = position + direction;
								agent.hasFacePoint = true;
								agent.stuckTimer = 0.0f;
							}
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

				// Held where it is for a moment, such as while getting up, though it keeps its path.
				if (agent.holdTimer > 0.0f)
				{
					agent.holdTimer -= deltaTime;
					agent.stuckTimer = 0.0f;
					desired = Vector3::c_Zero;
				}
				agent.velocity = MoveTowards(agent.velocity, desired, agent.acceleration * deltaTime);
				character.moveVelocity = agent.velocity;

				// Faces the way it's walking.
				const float speedSquared = agent.velocity.x * agent.velocity.x + agent.velocity.z * agent.velocity.z;
				if (speedSquared > 0.01f)
				{
					agent.yaw = TurnTowards(agent.yaw, Math::Atan2(agent.velocity.x, agent.velocity.z), maxTurn);
				}
				else if (agent.hasFacePoint && FlatDistance(position, agent.facePoint) > 0.05f)
				{
					agent.yaw = TurnTowards(agent.yaw, Math::Atan2(agent.facePoint.x - position.x, agent.facePoint.z - position.z), maxTurn);
				}
			});
		}
	}
}
