#pragma once

#include "RenderAPI/RenderAPI.h"

namespace tyr
{
	class TYR_GRAPHICS_API GraphicsManager 
	{
	public:
		static RenderAPI* CreateRenderAPI(const RenderAPICreateConfig& config);
		static void DestroyRenderAPI(RenderAPI* renderAPI);
	};
	
}