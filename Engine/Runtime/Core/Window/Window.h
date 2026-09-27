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
		WindowInputState input{};
	};
}