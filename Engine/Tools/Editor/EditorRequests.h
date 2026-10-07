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
	};
}
