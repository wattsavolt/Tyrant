#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "AssetSystem/AssetID.h"
#include "World/World.h"
#include "ECS/Components.h"

namespace tyr
{
	class WorldManager;

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

		// What an actor state starts with, ahead of its overrides.
		struct ActorStateHeader
		{
			Id64 id;
			Id64 typeID;
			ActorName name;
			RelativePath folder;
			Transform transform;
			// Where the overrides start in the state.
			uint overridesOffset;
		};

		// An actor in the same form as in the file, kept in memory for the editor's undo history.
		// Returns its size, or 0 if it doesn't fit in the buffer.
		static uint WriteActorState(const World& world, const ActorInstance& actor, uint8* buffer, uint capacity);
		static bool ReadActorStateHeader(const uint8* state, uint size, ActorStateHeader& header);
		// Adds the actor back to the world, returning its root entity or c_InvalidEntity.
		static Entity AddActorFromState(const uint8* state, uint size, WorldManager& worldManager, Handle worldHandle);

		// The level's settings and hierarchy folders, the same way. Returns 0 if it doesn't fit.
		static uint WriteLevelState(const World& world, uint8* buffer, uint capacity);
		static void ReadLevelState(const uint8* state, uint size, World& world);
#endif
	};
}
