#pragma once

#include "EditorMacros.h"

namespace tyr
{
	// Right panel showing the selected actor's component data.
	class TYR_EDITOR_EXPORT PropertiesPanel final
	{
	public:
		// Draws the panel's contents into the current window.
		void Draw();
	};
}
