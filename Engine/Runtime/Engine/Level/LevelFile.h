#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "AssetSystem/AssetID.h"

namespace tyr
{
	class WorldManager;
	struct World;

	// Reads and writes a level's source .level file, which stores each actor as its actor type plus
	// overrides. Paths are relative to the assets folder.
	class TYR_ENGINE_API LevelFile final
	{
	public:
		// Loads into a world with nothing in it. Returns false if the file can't be read.
		static bool Load(const char* levelPath, WorldManager& worldManager, Handle worldHandle);

#if TYR_EDITOR
		// Saves the world and registers the level along with the assets it uses. Returns the
		// level's asset ID, or an invalid ID if it couldn't be written.
		static AssetID Save(const char* levelPath, const World& world);
#endif
	};
}
