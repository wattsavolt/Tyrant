#pragma once

#include "Base/Base.h"
#include "Module/IModule.h"
#include "EngineMacros.h"

namespace tyr
{
	class InputManager;
	class TYR_ENGINE_EXPORT InputModule final : public IModule
	{
	public:
		InputModule();
		~InputModule();

		void InitializeModule() override;

		void ShutdownModule() override;

		void UpdateModule(float deltaTime) override;

		InputManager* GetInputManager() const { return m_InputManager; }

	private:
		InputManager* m_InputManager;
	};
	
}