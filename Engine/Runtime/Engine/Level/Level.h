#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "AssetSystem/AssetID.h"
#include "Math/Vector3.h"

namespace tyr
{
	struct Location
	{
		Array<AssetID> textures;
		Array<AssetID> materials;
		AssetID assetID;
		Vector3 offset;
	};
}