#pragma once

#include "Core.h"
#include "EngineMacros.h"

namespace tyr
{
	// Any application modules must be registered by this function
	using AppModulesRegistrationCallback = void (*)(void);
	using AppExitQueryCallback = FunctionRef<bool()>;

	class TYR_ENGINE_API EngineLoop final : INonCopyable
	{
	public:
		EngineLoop();
		~EngineLoop();

		// All modules will be registered and initialized here
		void Initialize(AppModulesRegistrationCallback registerAppModules);
		// All modules will be shutdown and unregistered here
		void Shutdown();
		void Run(AppExitQueryCallback shouldExit);

		bool IsInitialized() const { return m_Initialized; }

	private:
		bool m_Initialized;

		float m_LastFrameTime;
	};
	
}