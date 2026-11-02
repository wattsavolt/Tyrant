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

	void InputManager::UpdateLastDevice()
	{
		// A held gamepad stick or trigger counts, but a resting one drifting a little doesn't.
		constexpr float c_GamepadPush = 0.3f;
		bool keyboardOrMouse = m_RawMouseDelta.x != 0.0f || m_RawMouseDelta.y != 0.0f || m_ScrollDelta != 0.0f;
		for (uint i = 0; i < c_MaxKeyCodes && !keyboardOrMouse; ++i)
		{
			keyboardOrMouse = m_KeysDown[i] && !m_KeysDownPrev[i];
		}
		for (uint i = 0; i < c_MaxMouseButtons && !keyboardOrMouse; ++i)
		{
			keyboardOrMouse = m_MouseButtonsDown[i] && !m_MouseButtonsDownPrev[i];
		}
		const GamepadState& pad = m_Gamepads[0];
		const bool gamepad = pad.isConnected && ((pad.buttonsDown & ~pad.buttonsDownPrev) != 0
			|| pad.leftStick.Length() > c_GamepadPush || pad.rightStick.Length() > c_GamepadPush
			|| pad.leftTrigger > c_GamepadPush || pad.rightTrigger > c_GamepadPush);
		m_UsingGamepad = pad.isConnected && (gamepad || (m_UsingGamepad && !keyboardOrMouse));
	}

	void InputManager::SetMouseCaptured(bool captured)
	{
		if (m_WindowModule)
		{
			m_WindowModule->SetCursorCaptured(m_Window, captured);
		}
	}
}

	