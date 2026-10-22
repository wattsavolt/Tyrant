#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "ECS/EntitySystem.h"
#include "Gameplay/GameplayTypes.h"

namespace tyr
{
	struct World;
	struct InventoryComponent;
	struct DocumentsComponent;

	// Keeps what a character carries in its slots, stacking items that stack.
	namespace Inventory
	{
		// How many of the item it holds, across all its slots.
		TYR_ENGINE_API uint Count(const InventoryComponent& inventory, const ItemName& item);

		// Slots with nothing in them.
		TYR_ENGINE_API uint FreeSlots(const InventoryComponent& inventory);

		// How many are in the slot, and how many it holds.
		TYR_ENGINE_API uint SlotCount(const InventoryComponent& inventory, uint slot);
		TYR_ENGINE_API uint SlotStackLimit(const InventoryComponent& inventory, uint slot);

		// Puts up to count of the item in, topping up stacks it already has before taking new slots,
		// stackLimit to a slot. Returns how many fitted.
		TYR_ENGINE_API uint Add(InventoryComponent& inventory, const ItemName& item, uint count, uint stackLimit);

		// Takes up to count of the item out, emptying slots as they run out. Returns how many it took.
		TYR_ENGINE_API uint Remove(InventoryComponent& inventory, const ItemName& item, uint count);

		// Empties a slot, moving the ones after it up.
		TYR_ENGINE_API void RemoveSlot(InventoryComponent& inventory, uint slot);

		// Keeps a document, unless it's already kept. False when there's no room for it.
		TYR_ENGINE_API bool AddDocument(DocumentsComponent& documents, const ItemName& document);
		TYR_ENGINE_API bool HasDocument(const DocumentsComponent& documents, const ItemName& document);

		// Whether the character carries the item, or keeps it as a document.
		TYR_ENGINE_API bool Has(const World& world, Entity character, const ItemName& item);

		// When the character throws away spent keys, throws away one key whose locks are all
		// unlocked now, and says which. False when there's none.
		TYR_ENGINE_API bool DiscardSpentKey(World& world, Entity character, ItemName& outDiscarded);
	}
}
