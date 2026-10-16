#include "Doors.h"
#include "ECS/Components.h"
#include "World/World.h"
#include "Math/Math.h"
#include "Gameplay/Sounds.h"

namespace tyr
{
	namespace
	{
		void SetOpenSingle(World& world, Entity entity, bool open, const Vector3& userPosition)
		{
			Doors::CaptureClosedPose(world, entity);
			DoorComponent& door = world.entities.GetComponent<DoorComponent>(entity);
			const float target = open ? 1.0f : 0.0f;
			if (door.targetAmount == target)
			{
				return;
			}

			// The way to turn is only picked while it's shut, so it never flips mid-swing.
			if (open && door.openAmount == 0.0f && door.opensAway && static_cast<DoorMotion>(door.motion) == DoorMotion::Swing)
			{
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
			door.targetAmount = target;
			Sounds::Play(world, open ? door.openSound : door.closeSound, door.closedPosition);
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
			if (door.closedPoseKnown || !world.entities.HasComponent<ComponentTransform>(entity))
			{
				return;
			}
			const Transform& transform = world.entities.GetComponent<ComponentTransform>(entity).world;
			door.closedPosition = transform.position;
			door.closedRotation = transform.rotation;
			door.closedPoseKnown = true;
		}

		void SetOpen(World& world, Entity entity, bool open, const Vector3& userPosition)
		{
			if (!world.entities.HasComponent<DoorComponent>(entity))
			{
				return;
			}
			SetOpenSingle(world, entity, open, userPosition);
			const Entity linked = FindLinked(world, entity);
			if (linked != c_InvalidEntity)
			{
				SetOpenSingle(world, linked, open, userPosition);
			}
		}

		void Toggle(World& world, Entity entity, const Vector3& userPosition)
		{
			SetOpen(world, entity, !IsOpening(world, entity), userPosition);
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
				const Transform& transform = world.entities.GetComponent<ComponentTransform>(entity).world;
				closedPosition = transform.position;
				closedRotation = transform.rotation;
			}
			return closedRotation.Inverse().Rotate(position - closedPosition).z > 0.0f;
		}
	}
}
