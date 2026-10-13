#pragma once

#include "Core.h"
#include "Memory/PoolHandle.h"
#include "PropertiesReflector.h"

namespace tyr
{
	class WorldManager;

	// Edits the open level's settings, such as its sky and exposure, as undoable level edits.
	class LevelSettingsWindow final
	{
	public:
		LevelSettingsWindow(WorldManager& worldManager, Handle levelWorld);

		// Settings can only be changed while editing. open is cleared when the window is closed.
		void Draw(bool editing, bool& open);

	private:
		WorldManager& m_WorldManager;
		Handle m_LevelWorld;
		PropertiesReflector m_Reflector;
	};
}
