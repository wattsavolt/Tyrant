#include "InputModule.h"

#if TYR_PLATFORM == TYR_PLATFORM_WINDOWS
#include "Win32/PCInputManager.h"
#endif

#include "BuildConfig.h"

namespace tyr
{
	InputModule::InputModule()
	{
#if TYR_PLATFORM == TYR_PLATFORM_WINDOWS
		m_InputManager = new PCInputManager();
#endif
		//TODO: Other platforms
	}

	InputModule::~InputModule()
	{
		delete m_InputManager;
	}

	void InputModule::InitializeModule()
	{
		m_InputManager->Initialize();
	}

	void InputModule::UpdateModule(float deltaTime)
	{
		
	}

	void InputModule::ShutdownModule()
	{
		m_InputManager->Shutdown();
	}
}