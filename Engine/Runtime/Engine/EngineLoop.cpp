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
		// ModuleManager runs Update()/EndFrame() in REVERSE registration order, so RendererModule
		// (registered here, early) actually executes LATE in each phase - deliberately, so every
		// other module's Update() has already run by the time RendererModule::Update() (Renderer::
		// Render) does, including GUIModule's, which is what lets Render() safely dispatch
		// RenderAsync directly instead of deferring it - see Render()'s own comment. If GUIModule
		// (or AssetModule/WorldModule/AppModule, which also write into the current RenderFrame
		// slot before Render() runs) is ever registered AFTER RendererModule, that safety breaks
		// silently - no compile error, just the crash this ordering was fixed to prevent coming
		// back. Talk it through before reordering this list.
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
			// Time since the timer started
			const double currentTime = timer.GetMillisecondsPrecise();

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

		// Must happen before any module's Shutdown() below - some (AssetModule, deleting
		// AssetManager) delete objects that a still-running background task could be
		// referencing via a captured `this`, which would otherwise be a dangling-pointer
		// hazard. See TaskScheduler::WaitForAllTasks' own comment for why this is a single
		// global wait here rather than each module separately tracking and waiting on its own
		// tasks.
		TaskScheduler::Instance().WaitForAllTasks();

		// Every module's Shutdown() below tears down things (windows, scenes, assets, ...) that
		// in-flight render work might still reference. This is safe without an explicit early
		// flush because RemoveWindow/RemoveScene defer their actual work instead of touching
		// live data immediately - RendererModule's own Shutdown(), later in this same call (in
		// reverse registration order), is what actually waits for everything and processes the
		// deferred lists, by which point nothing before it needed that to have already happened.
		ModuleManager::Instance().ShutdownModules();

		m_Initialized = false;
	}
}