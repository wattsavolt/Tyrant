#pragma once

#include "EngineMacros.h"
#include "Core.h"

namespace tyr
{
	// Settings for a whole level, saved with it.
	struct LevelSettings
	{
		// Flat light added everywhere, until indirect lighting replaces it.
		float ambient = 0.15f;
	};
}
