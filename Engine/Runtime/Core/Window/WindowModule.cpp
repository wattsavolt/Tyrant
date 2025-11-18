#include "WindowModule.h"

#if TYR_PLATFORM == TYR_PLATFORM_WINDOWS
#include "Win32/PCWindow.h"
#endif

#include "BuildConfig.h"

namespace tyr
{
	WindowModule::WindowModule()
	{

	}

	WindowModule::~WindowModule()
	{
		
	}

	void WindowModule::InitializeModule()
	{
		// Get project related properties from the config later
		WindowProperties windowProperties;
		windowProperties.showFlag = 1;
		windowProperties.name = c_AppName;
		Window::GetMaxResolution(windowProperties.width, windowProperties.height);

#if TYR_PLATFORM == TYR_PLATFORM_WINDOWS
		m_PrimaryWindow = new PCWindow(windowProperties);
#endif
		//TODO: Other platforms
	}

	void WindowModule::UpdateModule(float deltaTime)
	{
		
	}

	void WindowModule::ShutdownModule()
	{
		delete m_PrimaryWindow;
	}
}