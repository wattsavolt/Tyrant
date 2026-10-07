#pragma once

#include "EditorMacros.h"
#include "Core.h"

namespace tyr
{
	// A search box over the list of every actor type, for choosing one to place.
	class TYR_EDITOR_EXPORT ActorPicker final
	{
	public:
		ActorPicker();

		// Returns true when an actor type is picked. focusSearch puts the keyboard in the search
		// box, such as when a popup opens. A listHeight of 0 fills the space left.
		bool Draw(Id64& pickedType, bool focusSearch, float listHeight);

	private:
		struct Entry
		{
			Id64 typeID;
			char displayName[48];
		};

		// Sorted by display name.
		Array<Entry> m_Entries;
		char m_Search[64] = {};
	};
}
