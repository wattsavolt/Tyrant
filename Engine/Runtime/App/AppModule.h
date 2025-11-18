#pragma once

#include "Core.h"
namespace tyr
{
	class AppBase;
	class AppModule final : public IModule
	{
	public:
		AppModule();
		~AppModule();

		void InitializeModule() override;

		void ShutdownModule() override;

		void UpdateModule(float deltaTime) override;

	private:
		AppBase* m_App;
	};
	
}