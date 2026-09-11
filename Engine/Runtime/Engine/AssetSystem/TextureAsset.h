#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "RenderResource/TextureDesc.h"
#include "AssetSystem/AssetID.h"

namespace tyr
{
	struct TextureHeader
	{
		TextureInfo info;
		size_t dataSize;
	};
}