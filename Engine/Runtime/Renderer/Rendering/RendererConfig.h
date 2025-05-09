

#pragma once

#include "RendererMacros.h"
#include "Core.h"
#include "RenderAPI/RenderAPI.h"
#include "DataTypes/DataTypes.h"

namespace tyr
{
	struct RendererConfig
	{
		RenderAPICreateConfig renderAPICreateConfig;
		RenderAPIConfig renderAPIConfig;
		String shaderDir;
		String shaderIncludeDir;
	};
}