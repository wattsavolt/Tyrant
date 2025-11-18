#pragma once

#include "Core.h"
#include "EngineMacros.h"

namespace tyr
{
	using AppModuleRegistrationCallback = void (*)(void);

	class TYR_ENGINE_EXPORT Engine final : INonCopyable
	{
	public:
		Engine();
		~Engine();

		void Initialize(AppModuleRegistrationCallback appModuleRegistrationCallback);
		void Shutdown();
		void Run();

		bool IsInitialized() const { return m_Initialized; }

	private:
		bool m_Initialized;

		float m_LastFrameTime;
	};
	
}