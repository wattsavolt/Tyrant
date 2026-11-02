#include "Interaction.h"
#include "Gameplay/Sounds.h"
#include "Doors.h"
#include "Power.h"
#include "ECS/Components.h"
#include "World/World.h"
#include "World/WorldManager.h"
#include "World/Camera.h"
#include "Input/InputManager.h"
#include "Debug/DebugDraw.h"
#include "Gameplay/Inventory.h"
#include <cstdio>

namespace tyr
{
	namespace
	{
		constexpr KeyCode c_KeyUse = 'E';
		constexpr float c_MessageDuration = 2.0f;
		// How far away enemies hear switches, panels and things being picked up, in metres.
		constexpr float c_UseNoise = 4.0f;

		InteractionHandler* g_Handler = nullptr;

		bool IsUsable(const World& world, Entity entity)
		{
			return world.entities.HasComponent<DoorComponent>(entity) || world.entities.HasComponent<DoorPanelComponent>(entity)
				|| world.entities.HasComponent<PickupComponent>(entity) || world.entities.HasComponent<LightSwitchComponent>(entity)
				|| world.entities.HasComponent<ActivatorComponent>(entity) || world.entities.HasComponent<ExamineComponent>(entity);
		}

		bool HasItem(const World& world, Entity user, const ItemName& item)
		{
			return Inventory::Has(world, user, item);
		}

		Vector3 GetPosition(const World& world, Entity entity)
		{
			return world.entities.HasComponent<TransformComponent>(entity) ? world.entities.GetComponent<TransformComponent>(entity).world.position : Vector3::c_Zero;
		}

		void SetMessage(InteractionText& message, const char* format, const char* value)
		{
			char text[InteractionText::c_Capacity];
			snprintf(text, sizeof(text), format, value);
			message = text;
		}

		// The thing's own remark, or the player's usual line when it has none.
		void SetRemark(InteractionText& message, const RemarkText& remark, const char* fallback)
		{
			message = remark.Size() > 0 ? remark.CStr() : fallback;
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
					SetRemark(outMessage, door.lockedRemark, "It's locked. Looks like I need a key.");
					Sounds::PlayHeard(world, door.lockedSound, GetPosition(world, target), door.noiseRadius * 0.5f);
					return false;
				}
				Doors::Unlock(world, target);
				break;
			case DoorLock::LockedFromBack:
				if (Doors::IsInFront(world, target, userPosition) == door.unlocksFromBack)
				{
					SetRemark(outMessage, door.lockedRemark, "It won't open. It must be locked from the other side.");
					Sounds::PlayHeard(world, door.lockedSound, GetPosition(world, target), door.noiseRadius * 0.5f);
					return false;
				}
				// Its key is left in the lock on this side.
				outMessage = "The key's in the lock. I unlocked it.";
				Doors::Unlock(world, target);
				break;
			case DoorLock::Locked:
				SetRemark(outMessage, door.lockedRemark, "It's locked. It won't budge.");
				Sounds::PlayHeard(world, door.lockedSound, GetPosition(world, target), door.noiseRadius * 0.5f);
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
			Sounds::PlayHeard(world, panel.useSound, GetPosition(world, target), c_UseNoise);
			if (!panel.powered && !Power::IsOn(world, GetPosition(world, target)))
			{
				SetRemark(outMessage, panel.unpoweredRemark, "It's dead. There's no power.");
				return false;
			}
			const Entity door = Doors::Find(world, panel.door);
			if (door == c_InvalidEntity)
			{
				outMessage = "Nothing happens.";
				return false;
			}
			// Once its door is unlocked, it opens without the key.
			if (panel.key.Size() > 0 && !(world.entities.HasComponent<DoorComponent>(door)
				&& world.entities.GetComponent<DoorComponent>(door).lock == static_cast<uint8>(DoorLock::Unlocked)))
			{
				if (!HasItem(world, user, panel.key))
				{
					SetRemark(outMessage, panel.lockedRemark, "It won't work. I need something to open this.");
					return false;
				}
				Doors::Unlock(world, door);
			}
			Doors::Toggle(world, door, GetPosition(world, user));
			return true;
		}

		bool UseLightSwitch(World& world, Entity target, InteractionText& outMessage)
		{
			Sounds::PlayHeard(world, world.entities.GetComponent<LightSwitchComponent>(target).useSound, GetPosition(world, target), c_UseNoise);
			Power::ToggleSwitch(world, target);
			if (!Power::IsOn(world, GetPosition(world, target)))
			{
				outMessage = "Nothing. The power must be out.";
			}
			return true;
		}

		bool UsePickup(WorldManager& worldManager, Handle worldHandle, World& world, Entity target, Entity user, InteractionText& outMessage)
		{
			PickupComponent& pickup = world.entities.GetComponent<PickupComponent>(target);
			uint taken = 0;
			if (pickup.document && world.entities.HasComponent<DocumentsComponent>(user))
			{
				taken = Inventory::AddDocument(world.entities.GetComponent<DocumentsComponent>(user), pickup.item) ? pickup.count : 0;
			}
			else if (world.entities.HasComponent<InventoryComponent>(user))
			{
				taken = Inventory::Add(world.entities.GetComponent<InventoryComponent>(user), pickup.item, pickup.count, pickup.stackLimit);
			}
			else
			{
				return false;
			}
			if (taken == 0)
			{
				outMessage = "I can't carry any more.";
				return false;
			}
			Sounds::PlayHeard(world, pickup.pickupSound, GetPosition(world, target), c_UseNoise);
			SetMessage(outMessage, "Took %s.", pickup.item.CStr());
			// Only what fits is taken, and the rest is left where it was.
			pickup.count = static_cast<uint8>(pickup.count - taken);
			if (pickup.count > 0)
			{
				outMessage = "I took what I could carry.";
				return true;
			}

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
		void SetHandler(InteractionHandler* handler)
		{
			g_Handler = handler;
		}

		bool UseOnlyAction(World& world, Entity user)
		{
			InteractionText action;
			if (!g_Handler || !g_Handler->GetOnlyAction(world, user, action))
			{
				return false;
			}
			g_Handler->UseOnlyAction(world, user);
			return true;
		}

		Entity FindUsable(const World& world, Entity hit)
		{
			Entity entity = hit;
			while (entity != c_InvalidEntity && world.entities.HasComponent<TransformComponent>(entity))
			{
				// The game's things come first, such as a hiding place used through its door.
				const Entity gameUsable = g_Handler ? g_Handler->FindUsable(world, entity) : c_InvalidEntity;
				if (gameUsable != c_InvalidEntity)
				{
					return gameUsable;
				}
				if (IsUsable(world, entity))
				{
					return entity;
				}
				entity = world.entities.GetComponent<TransformComponent>(entity).parentEntity;
			}
			return c_InvalidEntity;
		}

		void Describe(const World& world, Entity target, InteractionText& outText, const char* useButton)
		{
			char action[InteractionText::c_Capacity] = {};
			if (world.entities.HasComponent<PickupComponent>(target))
			{
				const PickupComponent& pickup = world.entities.GetComponent<PickupComponent>(target);
				if (pickup.count > 1)
				{
					snprintf(action, sizeof(action), "Take %s (%u)", pickup.item.CStr(), static_cast<uint>(pickup.count));
				}
				else
				{
					snprintf(action, sizeof(action), "Take %s", pickup.item.CStr());
				}
			}
			else if (world.entities.HasComponent<ActivatorComponent>(target))
			{
				snprintf(action, sizeof(action), "%s", world.entities.GetComponent<ActivatorComponent>(target).action.CStr());
			}
			else if (world.entities.HasComponent<DoorPanelComponent>(target))
			{
				snprintf(action, sizeof(action), "Use");
			}
			else if (world.entities.HasComponent<LightSwitchComponent>(target))
			{
				snprintf(action, sizeof(action), world.entities.GetComponent<LightSwitchComponent>(target).switchedOn ? "Lights off" : "Lights on");
			}
			else if (g_Handler && g_Handler->FindUsable(world, target) == target)
			{
				InteractionText gameAction;
				g_Handler->Describe(world, target, gameAction);
				if (gameAction.Size() == 0)
				{
					outText = {};
					return;
				}
				snprintf(action, sizeof(action), "%s", gameAction.CStr());
			}
			else if (world.entities.HasComponent<DoorComponent>(target))
			{
				snprintf(action, sizeof(action), Doors::IsOpening(world, target) ? "Close" : "Open");
			}
			else if (world.entities.HasComponent<ExamineComponent>(target))
			{
				snprintf(action, sizeof(action), "Examine");
			}
			else
			{
				outText = {};
				return;
			}
			char text[InteractionText::c_Capacity];
			snprintf(text, sizeof(text), "[%s] %s", useButton, action);
			outText = text;
		}

		bool Use(WorldManager& worldManager, Handle worldHandle, Entity target, Entity user, InteractionText& outMessage)
		{
			outMessage = {};
			World& world = worldManager.GetWorld(worldHandle);
			if (world.entities.HasComponent<PickupComponent>(target))
			{
				return UsePickup(worldManager, worldHandle, world, target, user, outMessage);
			}
			if (world.entities.HasComponent<ActivatorComponent>(target))
			{
				world.entities.GetComponent<ActivatorComponent>(target).used = true;
				return true;
			}
			if (world.entities.HasComponent<DoorPanelComponent>(target))
			{
				return UsePanel(world, target, user, outMessage);
			}
			if (world.entities.HasComponent<LightSwitchComponent>(target))
			{
				return UseLightSwitch(world, target, outMessage);
			}
			if (g_Handler && g_Handler->FindUsable(world, target) == target)
			{
				return g_Handler->Use(world, target, user, outMessage);
			}
			if (world.entities.HasComponent<DoorComponent>(target))
			{
				return UseDoor(world, target, user, outMessage);
			}
			if (world.entities.HasComponent<ExamineComponent>(target))
			{
				outMessage = world.entities.GetComponent<ExamineComponent>(target).remark.CStr();
				return true;
			}
			return false;
		}
	}

	namespace
	{
		// Small items are hard to hit exactly with the view's centre, so one close to it counts, as
		// long as nothing is in the way. Picks the one nearest the centre of view.
		Entity FindPickupInView(WorldManager& worldManager, World& world, const Vector3& eye, const Vector3& forward, Vector3& outPosition)
		{
			constexpr float c_MaxAngleCos = 0.978f;
			constexpr float c_BlockedSlack = 0.15f;
			Entity best = c_InvalidEntity;
			float bestCos = c_MaxAngleCos;
			world.entities.ForEach<PickupComponent>([&](Entity entity, PickupComponent& /*pickup*/)
			{
				if (!world.entities.HasComponent<TransformComponent>(entity))
				{
					return;
				}
				Vector3 centre = world.entities.GetComponent<TransformComponent>(entity).world.position;
				if (world.entities.HasComponent<ColliderComponent>(entity))
				{
					centre += world.entities.GetComponent<TransformComponent>(entity).world.rotation.Rotate(world.entities.GetComponent<ColliderComponent>(entity).center);
				}
				const Vector3 toItem = centre - eye;
				const float distance = toItem.Length();
				if (distance > GameplayConstants::c_InteractDistance || distance < 0.01f)
				{
					return;
				}
				const Vector3 direction = toItem / distance;
				const float cosAngle = Vector3::Dot(direction, forward);
				if (cosAngle <= bestCos)
				{
					return;
				}
				RayCastEntityHit hit;
				if (worldManager.GetPhysicsComponentsManager().Raycast(world.physicsWorld, eye, direction, distance, hit)
					&& Interaction::FindUsable(world, hit.entity) != entity && hit.distance < distance - c_BlockedSlack)
				{
					return;
				}
				best = entity;
				bestCos = cosAngle;
				outPosition = centre;
			});
			return best;
		}
	}

	void InteractionController::Update(WorldManager& worldManager, Handle worldHandle, Entity user, const InputManager& input, float deltaTime, bool hasInput)
	{
		World& world = worldManager.GetWorld(worldHandle);
		m_MessageTime = std::max(m_MessageTime - deltaTime, 0.0f);
		if (m_MessageTime > 0.0f && !m_MessageTaken)
		{
			DebugDraw::Text(m_MessagePosition, m_Message.CStr(), DebugColour::c_Orange);
		}
		if (!hasInput || !world.physicsWorld || user == c_InvalidEntity)
		{
			return;
		}

		RayCastEntityHit hit;
		const Vector3 eye = world.camera->GetPosition();
		const Vector3 forward = world.camera->GetForward();
		const bool usePressed = input.IsKeyPressed(c_KeyUse) || input.IsGamepadButtonPressed(GamepadButton::A);

		// While the game leaves only one thing to do, such as get out of hiding, that's all the button does.
		InteractionText onlyAction;
		if (g_Handler && g_Handler->GetOnlyAction(world, user, onlyAction))
		{
			if (m_MessageTime <= 0.0f)
			{
				char prompt[InteractionText::c_Capacity];
				snprintf(prompt, sizeof(prompt), "[%s] %s", input.IsUsingGamepad() ? "A" : "E", onlyAction.CStr());
				DebugDraw::Text(eye + forward * 0.6f, prompt, DebugColour::c_White);
			}
			if (usePressed)
			{
				g_Handler->UseOnlyAction(world, user);
			}
			return;
		}

		Entity target = c_InvalidEntity;
		if (worldManager.GetPhysicsComponentsManager().Raycast(world.physicsWorld, eye, forward, GameplayConstants::c_InteractDistance, hit))
		{
			target = Interaction::FindUsable(world, hit.entity);
		}
		if (target == c_InvalidEntity)
		{
			target = FindPickupInView(worldManager, world, eye, forward, hit.position);
		}
		if (target == c_InvalidEntity)
		{
			return;
		}

		if (m_MessageTime <= 0.0f)
		{
			InteractionText prompt;
			Interaction::Describe(world, target, prompt, input.IsUsingGamepad() ? "A" : "E");
			DebugDraw::Text(hit.position, prompt.CStr(), DebugColour::c_White);
		}

		if (usePressed)
		{
			InteractionText message;
			if (Interaction::Use(worldManager, worldHandle, target, user, message))
			{
				// Using a key can leave it with nothing more to unlock.
				ItemName discarded;
				if (Inventory::DiscardSpentKey(world, user, discarded))
				{
					SetMessage(message, "Discarded %s.", discarded.CStr());
				}
			}
			if (message.Size() > 0)
			{
				m_Message = message;
				m_MessagePosition = hit.position;
				m_MessageTime = c_MessageDuration;
				m_MessageNew = true;
				m_MessageTaken = false;
			}
		}
	}

	bool InteractionController::TakeMessage(InteractionText& outMessage)
	{
		if (!m_MessageNew)
		{
			return false;
		}
		outMessage = m_Message;
		m_MessageNew = false;
		m_MessageTaken = true;
		return true;
	}
}
