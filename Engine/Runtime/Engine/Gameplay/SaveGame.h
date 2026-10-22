#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "Memory/PoolHandle.h"
#include "String/Path.h"

namespace tyr
{
	struct World;
	class WorldManager;

	// A save holds the level being played and how its actors have changed since it was loaded:
	// which are gone, such as items taken, where characters and loose things are, and the state of
	// doors, lights and health. The game adds its own data, such as the player and its own
	// components, which it reads back after loading.
	namespace SaveGame
	{
		// The most the game can add to a save.
		constexpr uint c_MaxGameData = 16 * 1024;

		// Writes the world, played from the level at levelPath, to the file. False if it can't be written.
		TYR_ENGINE_API bool Write(const char* absPath, const World& world, const char* levelPath, const void* gameData, uint gameDataSize);

		// Loads the save's level into an empty world and puts its actors back how they were. The
		// game's data is copied out, and the level's path. False if the file isn't a save this can read.
		TYR_ENGINE_API bool Load(const char* absPath, WorldManager& worldManager, Handle worldHandle, uint8* gameData, uint gameDataCapacity,
			uint& outGameDataSize, RelativePath& outLevelPath);
	}
}
