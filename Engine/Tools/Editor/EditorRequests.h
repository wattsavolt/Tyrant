#pragma once

#include "Core.h"
#include "AssetSystem/AssetID.h"
#include "String/Path.h"

namespace tyr
{
	// What the editor's panels ask the editor to do this frame.
	struct EditorRequests
	{
		// An actor of actorType to place in folder.
		bool placeActor = false;
		Id64 actorType;
		// A static mesh actor for mesh to place in folder.
		bool placeMesh = false;
		AssetID mesh;
		// The hierarchy folder, empty for the root.
		RelativePath folder;

		// Level commands, carried out in this order. Saving comes first so a level can be saved
		// before another replaces it.
		bool saveLevel = false;
		// Everything with unsaved changes, which for now is the level.
		bool saveAll = false;
		bool saveLevelAs = false;
		bool newLevel = false;
		bool openLevel = false;
		// Makes level the one the editor opens on startup.
		bool setDefaultLevel = false;
		bool exit = false;
		// The name for Save As and New, without folder or extension.
		RelativePath levelName;
		AssetID level;
	};
}
