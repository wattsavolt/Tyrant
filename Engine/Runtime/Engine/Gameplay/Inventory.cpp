#include "Inventory.h"
#include "World/World.h"
#include "ECS/Components.h"
#include "Gameplay/Doors.h"
#include <algorithm>

namespace tyr
{
	namespace
	{
		// Items put in without a count, such as from a level, count as one each that doesn't stack.
		void FillCounts(InventoryComponent& inventory)
		{
			while (inventory.counts.Size() < inventory.items.Size())
			{
				inventory.counts.Add(1);
			}
			while (inventory.stackLimits.Size() < inventory.items.Size())
			{
				inventory.stackLimits.Add(1);
			}
		}

		uint SlotLimit(const InventoryComponent& inventory)
		{
			return std::min<uint>(inventory.slots, GameplayConstants::c_MaxInventoryItems);
		}

		// Whether the item opens a lock that's still locked, and whether it opens any lock at all.
		bool StillUnlocks(World& world, const ItemName& item, bool& outIsKey)
		{
			bool unlocks = false;
			world.entities.ForEach<DoorComponent>([&](Entity /*entity*/, const DoorComponent& door)
			{
				if (door.key == item)
				{
					outIsKey = true;
					unlocks |= door.lock == static_cast<uint8>(DoorLock::NeedsKey);
				}
			});
			world.entities.ForEach<DoorPanelComponent>([&](Entity /*entity*/, const DoorPanelComponent& panel)
			{
				if (panel.key != item)
				{
					return;
				}
				outIsKey = true;
				const Entity door = Doors::Find(world, panel.door);
				unlocks |= door != c_InvalidEntity && world.entities.HasComponent<DoorComponent>(door) && world.entities.GetComponent<DoorComponent>(door).lock != static_cast<uint8>(DoorLock::Unlocked);
			});
			return unlocks;
		}
	}

	namespace Inventory
	{
		uint Count(const InventoryComponent& inventory, const ItemName& item)
		{
			uint count = 0;
			for (uint i = 0; i < inventory.items.Size(); ++i)
			{
				if (inventory.items[i] == item)
				{
					count += SlotCount(inventory, i);
				}
			}
			return count;
		}

		uint FreeSlots(const InventoryComponent& inventory)
		{
			const uint limit = SlotLimit(inventory);
			return inventory.items.Size() < limit ? limit - inventory.items.Size() : 0;
		}

		uint SlotCount(const InventoryComponent& inventory, uint slot)
		{
			return slot < inventory.counts.Size() ? inventory.counts[slot] : 1;
		}

		uint SlotStackLimit(const InventoryComponent& inventory, uint slot)
		{
			return slot < inventory.stackLimits.Size() ? inventory.stackLimits[slot] : 1;
		}

		uint Add(InventoryComponent& inventory, const ItemName& item, uint count, uint stackLimit)
		{
			FillCounts(inventory);
			stackLimit = std::clamp<uint>(stackLimit, 1, 255);
			uint added = 0;
			for (uint i = 0; i < inventory.items.Size() && added < count; ++i)
			{
				if (inventory.items[i] == item && inventory.counts[i] < stackLimit)
				{
					const uint topUp = std::min<uint>(stackLimit - inventory.counts[i], count - added);
					inventory.counts[i] = static_cast<uint8>(inventory.counts[i] + topUp);
					inventory.stackLimits[i] = static_cast<uint8>(stackLimit);
					added += topUp;
				}
			}
			while (added < count && FreeSlots(inventory) > 0)
			{
				const uint stack = std::min<uint>(stackLimit, count - added);
				inventory.items.Add(item);
				inventory.counts.Add(static_cast<uint8>(stack));
				inventory.stackLimits.Add(static_cast<uint8>(stackLimit));
				added += stack;
			}
			return added;
		}

		uint Remove(InventoryComponent& inventory, const ItemName& item, uint count)
		{
			FillCounts(inventory);
			uint removed = 0;
			for (uint i = inventory.items.Size(); i-- > 0 && removed < count;)
			{
				if (inventory.items[i] != item)
				{
					continue;
				}
				const uint taken = std::min<uint>(inventory.counts[i], count - removed);
				inventory.counts[i] = static_cast<uint8>(inventory.counts[i] - taken);
				removed += taken;
				if (inventory.counts[i] == 0)
				{
					RemoveSlot(inventory, i);
				}
			}
			return removed;
		}

		void RemoveSlot(InventoryComponent& inventory, uint slot)
		{
			FillCounts(inventory);
			if (slot >= inventory.items.Size())
			{
				return;
			}
			inventory.items.Erase(slot);
			inventory.counts.Erase(slot);
			inventory.stackLimits.Erase(slot);
		}

		bool AddDocument(DocumentsComponent& documents, const ItemName& document)
		{
			if (HasDocument(documents, document))
			{
				return true;
			}
			if (documents.documents.Size() >= GameplayConstants::c_MaxDocuments)
			{
				return false;
			}
			documents.documents.Add(document);
			return true;
		}

		bool HasDocument(const DocumentsComponent& documents, const ItemName& document)
		{
			for (const ItemName& kept : documents.documents)
			{
				if (kept == document)
				{
					return true;
				}
			}
			return false;
		}

		bool Has(const World& world, Entity character, const ItemName& item)
		{
			return (world.entities.HasComponent<InventoryComponent>(character) && Count(world.entities.GetComponent<InventoryComponent>(character), item) > 0)
				|| (world.entities.HasComponent<DocumentsComponent>(character) && HasDocument(world.entities.GetComponent<DocumentsComponent>(character), item));
		}

		bool DiscardSpentKey(World& world, Entity character, ItemName& outDiscarded)
		{
			if (!world.entities.HasComponent<InventoryComponent>(character))
			{
				return false;
			}
			InventoryComponent& inventory = world.entities.GetComponent<InventoryComponent>(character);
			if (!inventory.discardSpentKeys)
			{
				return false;
			}
			for (uint i = 0; i < inventory.items.Size(); ++i)
			{
				bool isKey = false;
				if (!StillUnlocks(world, inventory.items[i], isKey) && isKey)
				{
					outDiscarded = inventory.items[i];
					RemoveSlot(inventory, i);
					return true;
				}
			}
			return false;
		}
	}
}
