#pragma once

#include "EngineMacros.h"

namespace tyr
{
	class AppBase;

	class TYR_ENGINE_EXPORT EngineManager 
	{
	public:
		static void RunEngine(const struct EngineParams& engineParams);
	};
	
}