#pragma once

#include "Core.h"
#include "EngineMacros.h"
#include "World/World.h"

namespace tyr
{
	class AppBase;

	// Should be loaded from a .ini file
	struct EngineProperties
	{
		uint maxTextures = 3000;
	};

	struct EngineParams
	{
		const char* appName = "";
		AppBase* app = nullptr;
		int windowShowFlag = 1;
	};

	class Window;
	class InputManager;
	class Renderer;
	struct WindowProperties;

	class TYR_ENGINE_EXPORT Engine final : INonCopyable
	{
	public:
		~Engine();

		virtual void Run();
		virtual void Stop();

		bool IsInitialized();

		bool IsRunning();

		uint AddWorld(const WorldParams& params);

		void RemoveWorld(uint index);

		void RemoveWorlds();

		///  Returns the properties of the mian window
		const WindowProperties& GetWindowProperties() const;

		World& GetWorld(uint index = 0)
		{
			TYR_ASSERT(index < Scene::c_MaxScenes && m_Worlds[index].IsInitialized());
			return m_Worlds[index];
		}

		static Engine* Instance() 
		{ 
			TYR_ASSERT(s_Instance);
			return s_Instance; 
		}

		// Returns width of the window's client area in pixels (width of swap chain)
		uint GetSurfaceWidth() const { return m_SurfaceWidth; }

		// Returns height of the window's client area in pixels (height of swap chain)
		uint GetSurfaceHeight() const { return m_SurfaceHeight; }


	private:
		friend class EngineManager;
		Engine(const EngineProperties& properties);

		void Initialize(const EngineParams& engineParams);
		void Shutdown();

		bool m_Initialized;
		bool m_Running;

		AppBase* m_App;
		URef<Window> m_MainWindow;
		URef<InputManager> m_InputManager;
		URef<Renderer> m_Renderer;
		FixedArray<World, Scene::c_MaxScenes> m_Worlds;
		RenderUpdateData m_RenderUpdateData;
		EngineProperties m_Properties;
		double m_LastFrameTime;
		uint m_SurfaceWidth;
		uint m_SurfaceHeight;
		uint m_WorldCount;

		static Engine* s_Instance;
	};
	
}