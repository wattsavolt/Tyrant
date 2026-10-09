#pragma once

#include "Base/Base.h"

namespace tyr
{
	constexpr uint c_MaxKeyCodes = 256;
	constexpr uint c_MaxMouseButtons = 3;
	constexpr uint c_MaxTypedCharsPerFrame = 16;

	// Raw OS input, filled in directly by the platform layer (e.g. PCWindow's WM_KEYDOWN/
	// WM_CHAR/mouse handling) as messages arrive. Key/button/position fields are live level
	// state, always safe to read. typedChars/scrollDelta accumulate between frames and must be
	// drained (read then cleared) by whoever consumes them - see WindowModule::ConsumeTypedChars/
	// ConsumeScrollDelta, which follow the same one-shot pattern as ConsumeResizePending below.
	struct WindowInputState
	{
		bool keysDown[c_MaxKeyCodes]{};
		bool mouseButtonsDown[c_MaxMouseButtons]{};
		int mouseX{};
		int mouseY{};
		float scrollDelta{};
		// Relative mouse motion from raw input (e.g. PCWindow's WM_INPUT handling), accumulated
		// between drains just like scrollDelta - see WindowModule::ConsumeRawMouseDelta. Unlike
		// mouseX/mouseY, this isn't affected by the cursor being clipped to the window or hidden,
		// so it's what a continuous look control (e.g. an editor fly camera) should read instead.
		int rawMouseDeltaX{};
		int rawMouseDeltaY{};
		char typedChars[c_MaxTypedCharsPerFrame]{};
		uint typedCharCount{};
	};

	struct Window
	{
		void* handle{};
		uint width{};
		uint height{};
		// Set by the platform layer (e.g. PCWindow's WM_SIZE handling) whenever the OS reports a
		// size change, cleared by WindowModule::ConsumeResizePending - a one-shot "has this
		// changed since it was last checked" flag, not a live "is it currently mid-resize" state.
		bool resizePending{};
		// Whether the cursor is currently hidden and confined to this window - see
		// WindowModule::SetCursorCaptured. Tracked here so the platform layer only calls into the
		// OS when the requested state actually changes.
		bool cursorCaptured{};
		// When set, closing the window only sets closeRequested, so the app can ask first.
		bool interceptClose{};
		bool closeRequested{};
		WindowInputState input{};
	};
}