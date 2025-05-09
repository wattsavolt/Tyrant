

#pragma once

#include "RenderAPI/RenderAPI.h"

namespace tyr
{
	class TYR_GRAPHICS_EXPORT GraphicsManager 
	{
	public:
		static Ref<RenderAPI> CreateRenderAPI(const RenderAPICreateConfig& config);
	};
	
}