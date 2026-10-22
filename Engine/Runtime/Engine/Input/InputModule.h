#pragma once

#include "Base/Base.h"
#include "Module/IModule.h"
#include "EngineMacros.h"

namespace tyr
{
	class InputManager;
	class TYR_ENGINE_API InputModule final : public IModule
	{
	public:
		InputModule();
		~InputModule();

		void Initialize() override;

		void Shutdown() override;

		// Latches this frame's input right after the window's messages are pumped, ahead of every
		// other module, so the app and the UI both see it this frame.
		void BeginFrame() override;

		void Update(float deltaTime) override;

		InputManager* GetInputManager() const { return m_InputManager; }

	private:
		InputManager* m_InputManager;
	};
	
}