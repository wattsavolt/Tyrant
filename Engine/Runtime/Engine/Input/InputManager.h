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

		// This tick's typed characters (WM_CHAR order), for text fields - cleared every Update().
		const char* GetTypedChars() const { return m_TypedChars; }
		uint GetTypedCharCount() const { return m_TypedCharCount; }

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
		float m_ScrollDelta = 0.0f;
		char m_TypedChars[c_MaxTypedCharsPerFrame]{};
		uint m_TypedCharCount = 0;
	};
}
