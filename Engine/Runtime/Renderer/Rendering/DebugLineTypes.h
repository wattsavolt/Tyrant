#pragma once

#include "Core.h"
#include "Math/Vector3.h"

namespace tyr
{
	// One end of a debug line. Matches DebugLineVS.hlsl byte for byte, with colour packed into
	// 4 bytes (R in the lowest byte).
	struct DebugLineVertex
	{
		Vector3 position;
		uint colour;
	};
}
