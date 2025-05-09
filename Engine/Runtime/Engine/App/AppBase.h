#pragma once

#include "Core.h"
#include "EngineMacros.h"

namespace tyr
{
	class Engine;
	class TYR_ENGINE_EXPORT AppBase : INonCopyable
	{
	public:
		AppBase();
		virtual ~AppBase() = default;

	protected:
		friend class Engine;
		// All called from the engine only
		virtual void Start();
		virtual void Update(double deltaTime) = 0;
		virtual void Stop();

		Engine* m_Engine;
	};
	
}