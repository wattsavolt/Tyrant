#pragma once

#include "Core.h"
#include "EngineMacros.h"

namespace tyr
{
	class Engine;
	class TYR_ENGINE_EXPORT AppBase 
	{
	public:
		virtual ~AppBase() = default;

		virtual void Initialize() = 0;

		// Called before engine shutdown
		virtual void Shutdown() = 0;

		// Called every frame by the engine
		virtual void Update(float deltaTime) = 0;

	protected:
		AppBase();

		friend class Engine;
		Engine* m_Engine;
	};
	
}