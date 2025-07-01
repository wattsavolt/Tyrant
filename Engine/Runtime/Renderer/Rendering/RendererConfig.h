

#pragma once

#include "RendererMacros.h"
#include "Core.h"
#include "RenderAPI/RenderAPI.h"
#include "RenderDataTypes/RenderDataTypes.h"

namespace tyr
{
	struct RendererConfig
	{
		RenderAPICreateConfig renderAPICreateConfig;
		RenderAPIConfig renderAPIConfig;
		String shaderDir;
		String shaderIncludeDir;
		bool voxelRendering = false;
	};
}