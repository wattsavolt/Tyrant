#pragma once

#include <Base/Base.h>
#include "EngineMacros.h"
#include "Window/WindowHandle.h"
#include "Window/Window.h"
#include "Math/Vector2.h"

namespace tyr
{
	class WindowModule;

	enum class MouseButton : uint8
	{
		Left,
		Right,
		Middle,
		Count
	};

	// Key codes are raw Win32 virtual-key codes (VK_*) on PC - see PCInputManager. Only one
	// platform exists today; a second platform would need its own code space here, but that's
	// deferred until one actually shows up rather than guessed at now.
	using KeyCode = uint8;

	constexpr uint c_MaxGamepads = 4;

	// Bitmask values for GamepadState::buttonsDown/buttonsDownPrev. A platform's own pad API maps
	// its native button bits into this generic set - see PCInputManager's XInput mapping.
	enum class GamepadButton : uint16
	{
		DPadUp = 0x0001,
		DPadDown = 0x0002,
		DPadLeft = 0x0004,
		DPadRight = 0x0008,
		Start = 0x0010,
		Back = 0x0020,
		LeftThumb = 0x0040,
		RightThumb = 0x0080,
		LeftShoulder = 0x0100,
		RightShoulder = 0x0200,
		A = 0x1000,
		B = 0x2000,
		X = 0x4000,
		Y = 0x8000,
	};

	// One controller's state, latched once per tick the same way keyboard/mouse state is. Stick
	// axes are deadzone-filtered and normalized to [-1, 1]; triggers are normalized to [0, 1].
	struct GamepadState
	{
		bool isConnected = false;
		uint16 buttonsDown = 0;
		uint16 buttonsDownPrev = 0;
		Vector2 leftStick{};
		Vector2 rightStick{};
		float leftTrigger = 0.0f;
		float rightTrigger = 0.0f;
	};

	// Queries this frame's input state, latched once per tick by Update() (called from
	// InputModule::Update()) from whatever the platform layer captured. Pressed/released are
	// edge-triggered (true for exactly the one tick the state changed); IsKeyDown/
	// IsMouseButtonDown are level state.
	class TYR_ENGINE_API InputManager : public INonCopyable
	{
	public:
		virtual ~InputManager();

		/// Initializes any input devices.
		virtual void Initialize() = 0;
		/// Deletes any input devices. */
		virtual void Shutdown() = 0;
		// Latches this tick's state from the platform layer - see the class comment.
		virtual void Update() = 0;

		// Lets whoever owns the OS window (Editor today) tell this manager which window's
		// input to read - same pattern as GUIModule::SetPrimaryWindow.
		void SetWindow(WindowModule* windowModule, WindowHandle window) { m_WindowModule = windowModule; m_Window = window; }

		bool IsKeyDown(KeyCode key) const { return m_KeysDown[key]; }
		bool IsKeyPressed(KeyCode key) const { return m_KeysDown[key] && !m_KeysDownPrev[key]; }
		bool IsKeyReleased(KeyCode key) const { return !m_KeysDown[key] && m_KeysDownPrev[key]; }

		bool IsMouseButtonDown(MouseButton button) const { return m_MouseButtonsDown[(uint8)button]; }
		bool IsMouseButtonPressed(MouseButton button) const { return m_MouseButtonsDown[(uint8)button] && !m_MouseButtonsDownPrev[(uint8)button]; }
		bool IsMouseButtonReleased(MouseButton button) const { return !m_MouseButtonsDown[(uint8)button] && m_MouseButtonsDownPrev[(uint8)button]; }

		const Vector2& GetMousePosition() const { return m_MousePos; }
		Vector2 GetMouseDelta() const { return m_MousePos - m_MousePosPrev; }
		float GetScrollDelta() const { return m_ScrollDelta; }

		// Relative mouse motion since the last Update(), from raw input rather than cursor
		// position - unaffected by the cursor being clipped to the window or hitting a screen
		// edge, so this is what a continuous look control (e.g. an editor fly camera) should use
		// instead of GetMouseDelta().
		const Vector2& GetRawMouseDelta() const { return m_RawMouseDelta; }

		// Hides and confines the cursor to the window for continuous look controls, or restores
		// normal cursor behaviour.
		void SetMouseCaptured(bool captured);

		// This tick's typed characters (WM_CHAR order), for text fields - cleared every Update().
		const char* GetTypedChars() const { return m_TypedChars; }
		uint GetTypedCharCount() const { return m_TypedCharCount; }

		bool IsGamepadConnected(uint gamepadIndex = 0) const { return GetGamepad(gamepadIndex).isConnected; }

		bool IsGamepadButtonDown(GamepadButton button, uint gamepadIndex = 0) const
		{
			return (GetGamepad(gamepadIndex).buttonsDown & (uint16)button) != 0;
		}

		bool IsGamepadButtonPressed(GamepadButton button, uint gamepadIndex = 0) const
		{
			const GamepadState& pad = GetGamepad(gamepadIndex);
			return (pad.buttonsDown & (uint16)button) != 0 && (pad.buttonsDownPrev & (uint16)button) == 0;
		}

		bool IsGamepadButtonReleased(GamepadButton button, uint gamepadIndex = 0) const
		{
			const GamepadState& pad = GetGamepad(gamepadIndex);
			return (pad.buttonsDown & (uint16)button) == 0 && (pad.buttonsDownPrev & (uint16)button) != 0;
		}

		const Vector2& GetGamepadLeftStick(uint gamepadIndex = 0) const { return GetGamepad(gamepadIndex).leftStick; }
		const Vector2& GetGamepadRightStick(uint gamepadIndex = 0) const { return GetGamepad(gamepadIndex).rightStick; }
		float GetGamepadLeftTrigger(uint gamepadIndex = 0) const { return GetGamepad(gamepadIndex).leftTrigger; }
		float GetGamepadRightTrigger(uint gamepadIndex = 0) const { return GetGamepad(gamepadIndex).rightTrigger; }

	protected:
		InputManager();

		WindowModule* m_WindowModule = nullptr;
		WindowHandle m_Window{};

		bool m_KeysDown[c_MaxKeyCodes]{};
		bool m_KeysDownPrev[c_MaxKeyCodes]{};
		bool m_MouseButtonsDown[c_MaxMouseButtons]{};
		bool m_MouseButtonsDownPrev[c_MaxMouseButtons]{};
		Vector2 m_MousePos{};
		Vector2 m_MousePosPrev{};
		Vector2 m_RawMouseDelta{};
		float m_ScrollDelta = 0.0f;
		char m_TypedChars[c_MaxTypedCharsPerFrame]{};
		uint m_TypedCharCount = 0;
		GamepadState m_Gamepads[c_MaxGamepads]{};

	private:
		const GamepadState& GetGamepad(uint gamepadIndex) const
		{
			TYR_ASSERT(gamepadIndex < c_MaxGamepads);
			return m_Gamepads[gamepadIndex];
		}
	};
}
