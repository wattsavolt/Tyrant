#include "Doors.h"
#include "ECS/Components.h"
#include "World/World.h"
#include "Math/Math.h"
#include "Gameplay/Sounds.h"
#include <algorithm>

namespace tyr
{
	namespace
	{
		// How far open a blow knocks a shut door before it falls back.
		constexpr float c_BashJolt = 0.15f;
		// Seconds a broken door takes to fall flat.
		constexpr float c_FallTime = 0.55f;

		// Turns a door that opens away from the user that way. Only while it's shut, so it never flips mid-swing.
		void TurnAwayFrom(World& world, Entity entity, DoorComponent& door, const Vector3& userPosition)
		{
			if (door.openAmount != 0.0f || !door.opensAway || static_cast<DoorMotion>(door.motion) != DoorMotion::Swing)
			{
				return;
			}
			// The middle of the leaf moves this way as it starts to turn.
			float leafX = 1.0f;
			if (world.entities.HasComponent<BoxComponent>(entity) && world.entities.GetComponent<BoxComponent>(entity).center.x != 0.0f)
			{
				leafX = world.entities.GetComponent<BoxComponent>(entity).center.x;
			}
			const Vector3 leaf(leafX, 0.0f, 0.0f);
			const Vector3 movement = Quaternion(Vector3::c_Up, door.openAngle * Math::c_DegToRad * 0.01f).Rotate(leaf) - leaf;
			const Vector3 toUser = door.closedRotation.Inverse().Rotate(userPosition - door.closedPosition);
			door.swingSign = movement.Dot(toUser) > 0.0f ? -1.0f : 1.0f;
		}

		void BashSingle(World& world, Entity entity, const Vector3& userPosition)
		{
			Doors::CaptureClosedPose(world, entity);
			DoorComponent& door = world.entities.GetComponent<DoorComponent>(entity);
			if (door.targetAmount > 0.0f || door.broken)
			{
				return;
			}
			TurnAwayFrom(world, entity, door, userPosition);
			door.openAmount = c_BashJolt;
		}

		void SetOpenSingle(World& world, Entity entity, bool open, const Vector3& userPosition, Entity user, bool burst)
		{
			Doors::CaptureClosedPose(world, entity);
			DoorComponent& door = world.entities.GetComponent<DoorComponent>(entity);
			const float target = open ? 1.0f : 0.0f;
			door.bursting = burst && open && !door.broken;
			if (door.targetAmount == target || door.broken)
			{
				return;
			}
			if (open)
			{
				TurnAwayFrom(world, entity, door, userPosition);
			}
			door.targetAmount = target;
			Sounds::PlayHeard(world, open ? door.openSound : door.closeSound, door.closedPosition, door.noiseRadius, user);
		}

		Entity FindLinked(const World& world, Entity entity)
		{
			const DoorComponent& door = world.entities.GetComponent<DoorComponent>(entity);
			if (door.linkedDoor.Size() == 0)
			{
				return c_InvalidEntity;
			}
			const Entity linked = Doors::Find(world, door.linkedDoor);
			return linked != entity ? linked : c_InvalidEntity;
		}
	}

	namespace Doors
	{
		void GetPose(const DoorComponent& door, Vector3& position, Quaternion& rotation)
		{
			// Broken off, it tips over about its bottom edge, faster as it goes, as if falling.
			if (door.broken)
			{
				const float fall = Math::Clamp01(door.fallAmount);
				position = door.closedPosition;
				rotation = door.closedRotation * Quaternion(Vector3::c_Right, Math::c_HalfPi * fall * fall * door.fallSign);
				return;
			}
			const float t = Math::Clamp01(door.openAmount);
			const float eased = t * t * (3.0f - 2.0f * t);
			if (static_cast<DoorMotion>(door.motion) == DoorMotion::Slide)
			{
				position = door.closedPosition + door.closedRotation.Rotate(door.slideOffset * eased);
				rotation = door.closedRotation;
				return;
			}
			const Quaternion swing(Vector3::c_Up, door.openAngle * Math::c_DegToRad * eased * door.swingSign);
			position = door.closedPosition;
			rotation = door.closedRotation * swing;
		}

		void CaptureClosedPose(World& world, Entity entity)
		{
			DoorComponent& door = world.entities.GetComponent<DoorComponent>(entity);
			if (door.closedPoseKnown || !world.entities.HasComponent<TransformComponent>(entity))
			{
				return;
			}
			const Transform& transform = world.entities.GetComponent<TransformComponent>(entity).world;
			door.closedPosition = transform.position;
			door.closedRotation = transform.rotation;
			door.closedPoseKnown = true;
		}

		void SetOpen(World& world, Entity entity, bool open, const Vector3& userPosition, Entity user)
		{
			if (!world.entities.HasComponent<DoorComponent>(entity))
			{
				return;
			}
			SetOpenSingle(world, entity, open, userPosition, user, false);
			const Entity linked = FindLinked(world, entity);
			if (linked != c_InvalidEntity)
			{
				SetOpenSingle(world, linked, open, userPosition, user, false);
			}
		}

		void BurstOpen(World& world, Entity entity, const Vector3& userPosition, Entity user)
		{
			if (!world.entities.HasComponent<DoorComponent>(entity))
			{
				return;
			}
			SetOpenSingle(world, entity, true, userPosition, user, true);
			const Entity linked = FindLinked(world, entity);
			if (linked != c_InvalidEntity)
			{
				SetOpenSingle(world, linked, true, userPosition, user, true);
			}
		}

		void Bash(World& world, Entity entity, const Vector3& userPosition, Entity user)
		{
			if (!world.entities.HasComponent<DoorComponent>(entity))
			{
				return;
			}
			BashSingle(world, entity, userPosition);
			const Entity linked = FindLinked(world, entity);
			if (linked != c_InvalidEntity)
			{
				BashSingle(world, linked, userPosition);
			}
			const DoorComponent& door = world.entities.GetComponent<DoorComponent>(entity);
			Sounds::PlayHeard(world, door.bashSound, door.closedPosition, door.noiseRadius, user);
		}

		void Break(World& world, Entity entity, const Vector3& fromPosition)
		{
			if (!world.entities.HasComponent<DoorComponent>(entity))
			{
				return;
			}
			CaptureClosedPose(world, entity);
			DoorComponent& door = world.entities.GetComponent<DoorComponent>(entity);
			if (door.broken)
			{
				return;
			}
			door.broken = true;
			door.fallAmount = 0.0f;
			// It falls whichever way tips its top towards what broke it.
			const Vector3 tippedUp = door.closedRotation.Rotate(Quaternion(Vector3::c_Right, Math::c_HalfPi).Rotate(Vector3::c_Up));
			door.fallSign = tippedUp.Dot(fromPosition - door.closedPosition) >= 0.0f ? 1.0f : -1.0f;
			door.openAmount = 0.0f;
			door.targetAmount = 0.0f;
		}

		bool UpdateFall(DoorComponent& door, float deltaTime)
		{
			if (!door.broken || door.fallAmount >= 1.0f)
			{
				return false;
			}
			door.fallAmount = std::min(door.fallAmount + deltaTime / c_FallTime, 1.0f);
			return true;
		}

		void Toggle(World& world, Entity entity, const Vector3& userPosition, Entity user)
		{
			SetOpen(world, entity, !IsOpening(world, entity), userPosition, user);
		}

		void Unlock(World& world, Entity entity)
		{
			if (!world.entities.HasComponent<DoorComponent>(entity))
			{
				return;
			}
			world.entities.GetComponent<DoorComponent>(entity).lock = static_cast<uint8>(DoorLock::Unlocked);
			const Entity linked = FindLinked(world, entity);
			if (linked != c_InvalidEntity)
			{
				world.entities.GetComponent<DoorComponent>(linked).lock = static_cast<uint8>(DoorLock::Unlocked);
			}
		}

		bool IsOpening(const World& world, Entity entity)
		{
			return world.entities.HasComponent<DoorComponent>(entity) && world.entities.GetComponent<DoorComponent>(entity).targetAmount > 0.5f;
		}

		Entity Find(const World& world, const ActorName& name)
		{
			for (const ActorInstance& actor : world.actorInstances)
			{
				if (actor.name == name && world.entities.HasComponent<DoorComponent>(actor.RootEntity()))
				{
					return actor.RootEntity();
				}
			}
			return c_InvalidEntity;
		}

		bool IsInFront(const World& world, Entity entity, const Vector3& position)
		{
			const DoorComponent& door = world.entities.GetComponent<DoorComponent>(entity);
			Vector3 closedPosition = door.closedPosition;
			Quaternion closedRotation = door.closedRotation;
			if (!door.closedPoseKnown)
			{
				const Transform& transform = world.entities.GetComponent<TransformComponent>(entity).world;
				closedPosition = transform.position;
				closedRotation = transform.rotation;
			}
			return closedRotation.Inverse().Rotate(position - closedPosition).z > 0.0f;
		}
	}
}
