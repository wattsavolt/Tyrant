#pragma once

#include "EngineMacros.h"
#include "Core.h"

namespace tyr
{
	class TYR_ENGINE_EXPORT AssetManager final
	{
	public:
		AssetManager() = default;
		void Update(float deltaTime);
	};
	
}