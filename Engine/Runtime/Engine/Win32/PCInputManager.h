#pragma once

#include "Input/InputManager.h"

#include <windows.h>

struct IGameInput;

namespace tyr
{
	class PCInputManager final : public InputManager
	{
	public:
		PCInputManager();
		~PCInputManager();

		void Initialize() override;
		void Shutdown() override;
		void Update() override;

	private:
		// Fills the first gamepad from GameInput when XInput has none there. Some controllers on
		// newer Windows only show up through GameInput.
		void UpdateGameInputGamepad();

		HINSTANCE hInstance;
		IGameInput* m_GameInput = nullptr;
		bool m_GameInputPadSeen = false;
	};
}
