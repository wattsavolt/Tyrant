#include "PCInputManager.h"
#include "Window/WindowModule.h"
#include "Window/Window.h"
#include "Math/Math.h"
#include <Xinput.h>
#include <cstring>

namespace tyr
{
	namespace
	{
		uint16 MapXInputButtons(WORD wButtons)
		{
			uint16 result = 0;
			if (wButtons & XINPUT_GAMEPAD_DPAD_UP) result |= (uint16)GamepadButton::DPadUp;
			if (wButtons & XINPUT_GAMEPAD_DPAD_DOWN) result |= (uint16)GamepadButton::DPadDown;
			if (wButtons & XINPUT_GAMEPAD_DPAD_LEFT) result |= (uint16)GamepadButton::DPadLeft;
			if (wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) result |= (uint16)GamepadButton::DPadRight;
			if (wButtons & XINPUT_GAMEPAD_START) result |= (uint16)GamepadButton::Start;
			if (wButtons & XINPUT_GAMEPAD_BACK) result |= (uint16)GamepadButton::Back;
			if (wButtons & XINPUT_GAMEPAD_LEFT_THUMB) result |= (uint16)GamepadButton::LeftThumb;
			if (wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) result |= (uint16)GamepadButton::RightThumb;
			if (wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) result |= (uint16)GamepadButton::LeftShoulder;
			if (wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) result |= (uint16)GamepadButton::RightShoulder;
			if (wButtons & XINPUT_GAMEPAD_A) result |= (uint16)GamepadButton::A;
			if (wButtons & XINPUT_GAMEPAD_B) result |= (uint16)GamepadButton::B;
			if (wButtons & XINPUT_GAMEPAD_X) result |= (uint16)GamepadButton::X;
			if (wButtons & XINPUT_GAMEPAD_Y) result |= (uint16)GamepadButton::Y;
			return result;
		}

		// Applies a radial deadzone, then rescales the remaining range to [0, 1] so a stick
		// barely past the deadzone doesn't jump straight to near-full magnitude.
		Vector2 NormalizeStick(SHORT rawX, SHORT rawY, SHORT deadzone)
		{
			const Vector2 stick((float)rawX, (float)rawY);
			const float magnitude = stick.Length();
			if (magnitude < (float)deadzone)
			{
				return Vector2::c_Zero;
			}

			const float normalizedMagnitude = Math::Clamp01((magnitude - deadzone) / (32767.0f - deadzone));
			return (stick / magnitude) * normalizedMagnitude;
		}

		float NormalizeTrigger(BYTE rawValue, BYTE threshold)
		{
			if (rawValue < threshold)
			{
				return 0.0f;
			}
			return Math::Clamp01((rawValue - threshold) / (255.0f - threshold));
		}
	}

	PCInputManager::PCInputManager()
	{
		hInstance = GetModuleHandle(nullptr);
	}

	PCInputManager::~PCInputManager()
	{

	}

	void PCInputManager::Initialize()
	{
		// XInput needs no explicit setup beyond linking xinput9_1_0.lib - XInputGetState works
		// standalone, and XInputEnable is deprecated/a no-op since Windows 8.
	}

	void PCInputManager::Shutdown()
	{

	}

	void PCInputManager::Update()
	{
		if (!m_WindowModule)
		{
			return;
		}

		memcpy(m_KeysDownPrev, m_KeysDown, sizeof(m_KeysDown));
		memcpy(m_MouseButtonsDownPrev, m_MouseButtonsDown, sizeof(m_MouseButtonsDown));
		m_MousePosPrev = m_MousePos;

		const WindowInputState& input = m_WindowModule->GetInputState(m_Window);
		memcpy(m_KeysDown, input.keysDown, sizeof(m_KeysDown));
		memcpy(m_MouseButtonsDown, input.mouseButtonsDown, sizeof(m_MouseButtonsDown));
		m_MousePos = Vector2((float)input.mouseX, (float)input.mouseY);

		m_ScrollDelta = m_WindowModule->ConsumeScrollDelta(m_Window);
		m_WindowModule->ConsumeTypedChars(m_Window, m_TypedChars, c_MaxTypedCharsPerFrame, m_TypedCharCount);

		int rawDeltaX = 0;
		int rawDeltaY = 0;
		m_WindowModule->ConsumeRawMouseDelta(m_Window, rawDeltaX, rawDeltaY);
		m_RawMouseDelta = Vector2(static_cast<float>(rawDeltaX), static_cast<float>(rawDeltaY));

		for (uint i = 0; i < c_MaxGamepads; ++i)
		{
			GamepadState& pad = m_Gamepads[i];
			pad.buttonsDownPrev = pad.buttonsDown;

			XINPUT_STATE state{};
			if (XInputGetState(i, &state) != ERROR_SUCCESS)
			{
				pad.isConnected = false;
				pad.buttonsDown = 0;
				pad.leftStick = Vector2::c_Zero;
				pad.rightStick = Vector2::c_Zero;
				pad.leftTrigger = 0.0f;
				pad.rightTrigger = 0.0f;
				continue;
			}

			pad.isConnected = true;
			pad.buttonsDown = MapXInputButtons(state.Gamepad.wButtons);
			pad.leftStick = NormalizeStick(state.Gamepad.sThumbLX, state.Gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
			pad.rightStick = NormalizeStick(state.Gamepad.sThumbRX, state.Gamepad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
			pad.leftTrigger = NormalizeTrigger(state.Gamepad.bLeftTrigger, XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
			pad.rightTrigger = NormalizeTrigger(state.Gamepad.bRightTrigger, XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
		}
	}
}
