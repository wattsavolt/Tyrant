#pragma once

#include "Input/InputManager.h"

#include <windows.h>

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
		HINSTANCE hInstance;
	};
}
