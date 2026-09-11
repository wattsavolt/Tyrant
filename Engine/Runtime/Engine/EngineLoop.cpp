#include "EngineLoop.h"
#include "Window/Window.h"
#include "Window/WindowModule.h"
#include "Input/InputManager.h"
#include "Input/InputModule.h"
#include "AssetSystem/AssetManager.h"
#include "AssetSystem/AssetModule.h"
#include "World/WorldManager.h"
#include "World/WorldModule.h"
#include "RendererModule.h"
#include "BuildConfig.h"
#include "World/Camera.h"
#include "Time/Timer.h"
#include "Math/Vector2.h"
#include "RenderAPI/Device.h"

namespace tyr
{
	EngineLoop::EngineLoop()
		: m_Initialized(false)
		, m_LastFrameTime(0)
	{

	}

	EngineLoop::~EngineLoop()
	{
		if (m_Initialized)
		{
			Shutdown();
		}
	}

	void EngineLoop::Initialize(AppModulesRegistrationCallback registerAppModules)
	{
		TYR_ASSERT(!m_Initialized);
		
		TYR_REGISTER_MODULE(WindowModule);
		TYR_REGISTER_MODULE(RendererModule);
		TYR_REGISTER_MODULE(InputModule);
		TYR_REGISTER_MODULE(AssetModule);
		TYR_REGISTER_MODULE(WorldModule);

		// Register any extra app modules
		if (registerAppModules)
		{
			registerAppModules();
		}

		ModuleManager::Instance().InitializeModules();

		m_Initialized = true;
	}

	void EngineLoop::Run(AppExitQueryCallback shouldExit)
	{
		TYR_ASSERT(m_Initialized);

		Timer timer;

		ModuleManager& moduleManager = ModuleManager::Instance();
		while (shouldExit())
		{
			// Time since the timer started
			const double currentTime = timer.GetMillisecondsPrecise();

			const float deltaTime = static_cast<float>(currentTime - m_LastFrameTime);

			moduleManager.BeginFrame();
			moduleManager.Update(deltaTime);
			moduleManager.EndFrame();

			m_LastFrameTime = currentTime;
		}
	}

	void EngineLoop::Shutdown()
	{
		TYR_ASSERT(m_Initialized);

		ModuleManager::Instance().ShutdownModules();

		m_Initialized = false;
	}
}