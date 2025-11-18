#include "Engine.h"
#include "App/AppBase.h"
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
	Engine::Engine()
		: m_Initialized(false)
		, m_LastFrameTime(0)
	{

	}

	Engine::~Engine()
	{
		if (m_Initialized)
		{
			Shutdown();
		}
	}

	void Engine::Initialize(AppModuleRegistrationCallback appModuleRegistrationCallback)
	{
		TYR_ASSERT(!m_Initialized);
		
		TYR_REGISTER_MODULE(WindowModule);
		TYR_REGISTER_MODULE(RendererModule);
		TYR_REGISTER_MODULE(InputModule);
		TYR_REGISTER_MODULE(AssetModule);
		TYR_REGISTER_MODULE(WorldModule);

		// Register any app modules
		if (appModuleRegistrationCallback)
		{
			appModuleRegistrationCallback();
		}

		ModuleManager::Instance().InitializeModules();

		m_Initialized = true;
	}

	void Engine::Run()
	{
		TYR_ASSERT(m_Initialized);

		WindowModule* windowModule;
		TYR_GET_MODULE(WindowModule, windowModule);
		Window* primaryWindow = windowModule->GetPrimaryWindow();
		Timer timer;

		while (primaryWindow->IsActive())
		{
			// Time since the timer started
			const double currentTime = timer.GetMillisecondsPrecise();

			const float deltaTime = static_cast<float>(currentTime - m_LastFrameTime);

			ModuleManager::Instance().UpdateModules(deltaTime);

			m_LastFrameTime = currentTime;

			primaryWindow->PollEvents();
		}
	}

	void Engine::Shutdown()
	{
		TYR_ASSERT(m_Initialized);

		ModuleManager::Instance().ShutdownModules();

		m_Initialized = false;
	}
}