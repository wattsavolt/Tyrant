#pragma once

#include "EditorMacros.h"

namespace tyr
{
	// The Hierarchy tab, listing the actors in the level.
	class TYR_EDITOR_EXPORT LevelHierarchyPanel final
	{
	public:
		// Draws the tab's contents into the current window.
		void Draw();
	};
}
