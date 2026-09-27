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

		// Latches this frame's input here rather than in Update() - BeginFrame/Update/EndFrame
		// each run as a full pass over every module before the next phase starts, and
		// registration order (this module comes right after WindowModule, whose own BeginFrame
		// pumps OS messages) puts this ahead of GUIModule's BeginFrame in that same reverse-order
		// pass, so GUIModule sees this tick's input by the time it reads it. See WindowModule::
		// BeginFrame's own comment for the same reasoning.
		void BeginFrame() override;

		void Update(float deltaTime) override;

		InputManager* GetInputManager() const { return m_InputManager; }

	private:
		InputManager* m_InputManager;
	};
	
}