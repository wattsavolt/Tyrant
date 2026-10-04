#include "InputManager.h"

#include "Core.h"
#include "Window/WindowModule.h"

#if TYR_PLATFORM == TYR_PLATFORM_WINDOWS
#include "Win32/PCInputManager.h"
#endif

namespace tyr
{
	InputManager::InputManager()
	{

	}

	InputManager::~InputManager()
	{

	}

	void InputManager::SetMouseCaptured(bool captured)
	{
		if (m_WindowModule)
		{
			m_WindowModule->SetCursorCaptured(m_Window, captured);
		}
	}
}

	