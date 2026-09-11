#pragma once

#include "Core.h"
#include "EngineMacros.h"

namespace tyr
{
	class TYR_ENGINE_API AppBase 
	{
	public:
		virtual ~AppBase() = default;

		virtual void Initialize() = 0;

		// Called before engine shutdown
		virtual void Shutdown() = 0;

		// Called every frame by the engine
		virtual void Update(float deltaTime) = 0;

		virtual bool WantsExit() const = 0;

	protected:
		AppBase();
	};
	
}