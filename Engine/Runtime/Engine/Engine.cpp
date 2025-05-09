#include "Engine.h"
#include "App/AppBase.h"
#include "Window/Window.h"
#include "Input/InputManager.h"
#include "Rendering/Renderer.h"
#include "EngineConfig.h"
#include "World/Camera.h"
#include "Time/Timer.h"
#include "Math/Vector2.h"

namespace tyr
{
	Engine* Engine::s_Instance = nullptr;

	Engine::Engine()
		: m_Initialized(false)
		, m_Running(false)
		, m_App(nullptr)
		, m_WorldCount(0)
		, m_LastFrameTime(0)
		, m_SurfaceWidth(0)
		, m_SurfaceHeight(0)
	{
		TYR_ASSERT(!s_Instance);
		s_Instance = this;
	}

	Engine::~Engine()
	{
		if (m_Initialized)
		{
			Shutdown();
		}

		s_Instance = nullptr;
	}

	void Engine::Initialize(const EngineParams& engineParams)
	{
		TYR_ASSERT(!m_Initialized);

		m_App = engineParams.app;

		// Load project related properties from file later
		// Resource path should be taken from the app
		WindowProperties windowProperties;
		windowProperties.showFlag = engineParams.windowShowFlag;
		windowProperties.name = engineParams.appName;
		Window::GetMaxResolution(windowProperties.width, windowProperties.height);

		m_MainWindow = CreateURef(Window::Create());
		m_MainWindow->Initialize(windowProperties);

		m_InputManager = CreateURef(InputManager::Create());
		m_InputManager->Initialize();
		
		m_Renderer = MakeURef<Renderer>();
		RendererConfig rendererConfig;
		rendererConfig.renderAPIConfig.windowHandle = m_MainWindow->GetHandle();
		rendererConfig.renderAPIConfig.appName = engineParams.appName;
		rendererConfig.renderAPICreateConfig.backend = RenderAPIBackend::Vulkan;

#if TYR_IS_FINAL
		rendererConfig.shaderDir = Platform::GetBinaryDirectory() + "/Shaders/";
#else
		rendererConfig.shaderDir = c_EngineShadersDir;
#endif
		rendererConfig.shaderIncludeDir = rendererConfig.shaderDir;

		m_Renderer->Initialize(rendererConfig, m_SurfaceWidth, m_SurfaceHeight);

		m_Initialized = true;
	}

	void Engine::Run()
	{
		TYR_ASSERT(m_Initialized && !m_Running);

		m_Running = true;

		Timer timer;

		m_App->Start();

		bool quitMsgReceived;
		while (m_Running)
		{
			// Time since the timer started
			const double currentTime = timer.GetMillisecondsPrecise();
			if (m_InputManager->HandleNextMessage(quitMsgReceived))
			{
				if (quitMsgReceived)
				{
					break;
				}
				continue;
			}
			
			const double deltaTime = currentTime - m_LastFrameTime;

			m_App->Update(deltaTime);

			for (uint i = 0; i < Scene::c_MaxScenes; ++i)
			{
				if (m_Worlds[i].IsInitialized())
				{
					if (!m_Worlds[i].IsFirstFrame())
					{
						m_RenderUpdateData.sceneActions[i].action = SceneActionType::Update;
					}
					else
					{
						m_RenderUpdateData.sceneActions[i].action = SceneActionType::Add;
						m_Worlds[i].SetIsFirstFrame(false);
					}
					
					m_RenderUpdateData.sceneActions[i].viewArea = m_Worlds[i].GetViewArea();
					m_RenderUpdateData.sceneActions[i].cameraPosition = m_Worlds[i].GetCamera()->GetPosition();
					m_RenderUpdateData.sceneActions[i].viewProj = m_Worlds[i].GetCamera()->GetViewProjection();
				}
			}

			m_Renderer->Render(deltaTime, m_RenderUpdateData);

			m_LastFrameTime = currentTime;
		}
	}

	void Engine::Stop()
	{
		TYR_ASSERT(m_Running);
		m_App->Stop();
		m_Running = false;
		m_RenderUpdateData = {};
	}

	void Engine::Shutdown()
	{
		TYR_ASSERT(m_Initialized);

		if (m_Running)
		{
			Stop();
		}

		m_Renderer->Shutdown();

		RemoveWorlds();

		m_Initialized = false;
	}

	bool Engine::IsInitialized()
	{
		return m_Initialized;
	}

	bool Engine::IsRunning()
	{
		return m_Running;
	}

	uint Engine::AddWorld(const WorldParams& params)
	{
		TYR_ASSERT(m_WorldCount < Scene::c_MaxScenes - 1);
		for (uint i = 0; i < Scene::c_MaxScenes; ++i)
		{
			if (!m_Worlds[i].IsInitialized())
			{
				m_Worlds[i].Initialize(params);
				m_WorldCount++;
				return i;
			}
		}
		TYR_ASSERT(false);
		return UINT32_MAX;
	}

	void Engine::RemoveWorld(uint index)
	{
		TYR_ASSERT(index >= 0 && index < Scene::c_MaxScenes);
		if (m_Worlds[index].IsInitialized())
		{
			m_RenderUpdateData.sceneActions[index].action = SceneActionType::Remove;
			m_Worlds[index].Shutdown();
			m_WorldCount--;
		}
	}

	void Engine::RemoveWorlds()
	{
		for (int i = 0; i < Scene::c_MaxScenes; ++i)
		{
			RemoveWorld(i);
		}
	}

	const WindowProperties& Engine::GetWindowProperties() const
	{
		return m_MainWindow->GetProperties();;
	}
}