

#pragma once

#include "RendererMacros.h"
#include "Core.h"
#include "RenderAPI/RenderAPI.h"
#include "RenderDataTypes/RenderDataTypes.h"
#include "Shader/ShaderCreator.h"

namespace tyr
{
	struct RendererConfig
	{
		RenderAPICreateConfig renderAPICreateConfig;
		RenderAPIConfig renderAPIConfig;
		ShaderCreatorConfig shaderConfig;
		bool voxelRendering = false;
	};
}