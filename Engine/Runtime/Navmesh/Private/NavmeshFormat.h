#pragma once

#include "Core.h"

namespace tyr
{
	// How navmesh data is laid out: this header, then each tile as its size followed by Detour's
	// own tile data.
	namespace NavmeshFormat
	{
		struct Header
		{
			uint version = 0;
			// Where tile 0, 0 starts, and each tile's size across.
			float origin[3] = {};
			float tileWidth = 0.0f;
			float tileHeight = 0.0f;
			uint maxTiles = 0;
			uint maxPolysPerTile = 0;
			uint tileCount = 0;
		};
	}
}
