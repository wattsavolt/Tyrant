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
#include "GUI/GUIModule.h"
#include "BuildConfig.h"
#include "World/Camera.h"
#include "Time/Timer.h"
#include "Math/Vector2.h"
#include "RenderAPI/Device.h"
#include "Threading/TaskScheduler.h"

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

		// Starts the task scheduler's worker threads up front, before any module runs.
		TaskScheduler::Instance();

		TYR_REGISTER_MODULE(WindowModule);
		TYR_REGISTER_MODULE(RendererModule);
		// ModuleManager runs Update()/EndFrame() in reverse registration order, so a module
		// registered here, early, executes late in each phase. RendererModule must run after
		// every module that writes into the current render frame, so keep it registered before them.
		TYR_REGISTER_MODULE(GUIModule);
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
			// Time since the timer started, in seconds - every module's Update(deltaTime) and
			// everything derived from it (e.g. the editor camera's move speed) expects seconds.
			const double currentTime = timer.GetSecondsPrecise();

			const float deltaTime = static_cast<float>(currentTime - m_LastFrameTime);

			moduleManager.BeginFrame();
			moduleManager.Update(deltaTime);
			moduleManager.EndFrame();

			// Returns any task slots the main thread has freed this frame back to the
			// shared pool. Worker threads do this automatically while idle; the main
			// thread has no equivalent idle point, so it needs an explicit periodic flush.
			TaskScheduler::Instance().FlushCurrentThreadCache();

			m_LastFrameTime = currentTime;
		}
	}

	void EngineLoop::Shutdown()
	{
		TYR_ASSERT(m_Initialized);

		// Must happen before any module's Shutdown() below - some modules delete objects that
		// a still-running background task could be referencing via a captured `this`, which
		// would otherwise be a dangling-pointer hazard.
		TaskScheduler::Instance().WaitForAllTasks();

		// Every module's Shutdown() below tears down things that in-flight render work might
		// still reference. This is safe without an explicit early flush because teardown defers
		// its actual work instead of touching live data immediately, processed later in this same call.
		ModuleManager::Instance().ShutdownModules();

		m_Initialized = false;
	}
}