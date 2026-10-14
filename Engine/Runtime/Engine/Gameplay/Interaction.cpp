#include "Interaction.h"
#include "Doors.h"
#include "ECS/Components.h"
#include "World/World.h"
#include "World/WorldManager.h"
#include "World/Camera.h"
#include "Input/InputManager.h"
#include "Debug/DebugDraw.h"
#include <cstdio>

namespace tyr
{
	namespace
	{
		constexpr KeyCode c_KeyUse = 'E';
		constexpr float c_MessageDuration = 2.0f;

		bool IsUsable(const World& world, Entity entity)
		{
			return world.entities.HasComponent<DoorComponent>(entity) || world.entities.HasComponent<DoorPanelComponent>(entity)
				|| world.entities.HasComponent<PickupComponent>(entity);
		}

		bool HasItem(const World& world, Entity user, const ItemName& item)
		{
			if (!world.entities.HasComponent<InventoryComponent>(user))
			{
				return false;
			}
			for (const ItemName& held : world.entities.GetComponent<InventoryComponent>(user).items)
			{
				if (held == item)
				{
					return true;
				}
			}
			return false;
		}

		Vector3 GetPosition(const World& world, Entity entity)
		{
			return world.entities.HasComponent<ComponentTransform>(entity) ? world.entities.GetComponent<ComponentTransform>(entity).world.position : Vector3::c_Zero;
		}

		void SetMessage(InteractionText& message, const char* format, const char* value)
		{
			char text[InteractionText::c_Capacity];
			snprintf(text, sizeof(text), format, value);
			message = text;
		}

		bool UseDoor(World& world, Entity target, Entity user, InteractionText& outMessage)
		{
			DoorComponent& door = world.entities.GetComponent<DoorComponent>(target);
			const Vector3 userPosition = GetPosition(world, user);
			switch (static_cast<DoorLock>(door.lock))
			{
			case DoorLock::NeedsKey:
				if (!HasItem(world, user, door.key))
				{
					SetMessage(outMessage, "Locked. Needs %s.", door.key.CStr());
					return false;
				}
				Doors::Unlock(world, target);
				break;
			case DoorLock::LockedFromBack:
				if (!Doors::IsInFront(world, target, userPosition))
				{
					outMessage = "Locked from the other side.";
					return false;
				}
				Doors::Unlock(world, target);
				break;
			case DoorLock::Locked:
				outMessage = "Locked.";
				return false;
			default:
				break;
			}
			Doors::Toggle(world, target, userPosition);
			return true;
		}

		bool UsePanel(World& world, Entity target, Entity user, InteractionText& outMessage)
		{
			const DoorPanelComponent& panel = world.entities.GetComponent<DoorPanelComponent>(target);
			if (!panel.powered)
			{
				outMessage = "No power.";
				return false;
			}
			const Entity door = Doors::Find(world, panel.door);
			if (door == c_InvalidEntity)
			{
				outMessage = "Nothing happens.";
				return false;
			}
			if (panel.key.Size() > 0)
			{
				if (!HasItem(world, user, panel.key))
				{
					SetMessage(outMessage, "Needs %s.", panel.key.CStr());
					return false;
				}
				Doors::Unlock(world, door);
			}
			Doors::Toggle(world, door, GetPosition(world, user));
			return true;
		}

		bool UsePickup(WorldManager& worldManager, Handle worldHandle, World& world, Entity target, Entity user, InteractionText& outMessage)
		{
			if (!world.entities.HasComponent<InventoryComponent>(user))
			{
				return false;
			}
			InventoryComponent& inventory = world.entities.GetComponent<InventoryComponent>(user);
			if (inventory.items.Size() >= GameplayConstants::c_MaxInventoryItems)
			{
				outMessage = "Can't carry any more.";
				return false;
			}
			const ItemName item = world.entities.GetComponent<PickupComponent>(target).item;
			inventory.items.Add(item);
			SetMessage(outMessage, "Took %s.", item.CStr());

			for (const ActorInstance& actor : world.actorInstances)
			{
				for (Entity entity : actor.entities)
				{
					if (entity == target)
					{
						worldManager.RemoveActorInstance(worldHandle, actor.RootEntity());
						return true;
					}
				}
			}
			return true;
		}
	}

	namespace Interaction
	{
		Entity FindUsable(const World& world, Entity hit)
		{
			Entity entity = hit;
			while (entity != c_InvalidEntity && world.entities.HasComponent<ComponentTransform>(entity))
			{
				if (IsUsable(world, entity))
				{
					return entity;
				}
				entity = world.entities.GetComponent<ComponentTransform>(entity).parentEntity;
			}
			return c_InvalidEntity;
		}

		void Describe(const World& world, Entity target, InteractionText& outText)
		{
			if (world.entities.HasComponent<PickupComponent>(target))
			{
				SetMessage(outText, "[E] Take %s", world.entities.GetComponent<PickupComponent>(target).item.CStr());
			}
			else if (world.entities.HasComponent<DoorPanelComponent>(target))
			{
				outText = "[E] Use";
			}
			else if (world.entities.HasComponent<DoorComponent>(target))
			{
				outText = Doors::IsOpening(world, target) ? "[E] Close" : "[E] Open";
			}
			else
			{
				outText = {};
			}
		}

		bool Use(WorldManager& worldManager, Handle worldHandle, Entity target, Entity user, InteractionText& outMessage)
		{
			outMessage = {};
			World& world = worldManager.GetWorld(worldHandle);
			if (world.entities.HasComponent<PickupComponent>(target))
			{
				return UsePickup(worldManager, worldHandle, world, target, user, outMessage);
			}
			if (world.entities.HasComponent<DoorPanelComponent>(target))
			{
				return UsePanel(world, target, user, outMessage);
			}
			if (world.entities.HasComponent<DoorComponent>(target))
			{
				return UseDoor(world, target, user, outMessage);
			}
			return false;
		}
	}

	void InteractionController::Update(WorldManager& worldManager, Handle worldHandle, Entity user, const InputManager& input, float deltaTime, bool hasInput)
	{
		World& world = worldManager.GetWorld(worldHandle);
		m_MessageTime = std::max(m_MessageTime - deltaTime, 0.0f);
		if (m_MessageTime > 0.0f)
		{
			DebugDraw::Text(m_MessagePosition, m_Message.CStr(), DebugColour::c_Orange);
		}
		if (!hasInput || !world.physicsWorld || user == c_InvalidEntity)
		{
			return;
		}

		RaycastHit hit;
		const Vector3 eye = world.camera->GetPosition();
		if (!worldManager.GetPhysicsManager().Raycast(world.physicsWorld, eye, world.camera->GetForward(), GameplayConstants::c_InteractDistance, hit))
		{
			return;
		}
		const Entity target = Interaction::FindUsable(world, hit.entity);
		if (target == c_InvalidEntity)
		{
			return;
		}

		if (m_MessageTime <= 0.0f)
		{
			InteractionText prompt;
			Interaction::Describe(world, target, prompt);
			DebugDraw::Text(hit.position, prompt.CStr(), DebugColour::c_White);
		}

		if (input.IsKeyPressed(c_KeyUse) || input.IsGamepadButtonPressed(GamepadButton::X))
		{
			InteractionText message;
			Interaction::Use(worldManager, worldHandle, target, user, message);
			if (message.Size() > 0)
			{
				m_Message = message;
				m_MessagePosition = hit.position;
				m_MessageTime = c_MessageDuration;
			}
		}
	}
}
