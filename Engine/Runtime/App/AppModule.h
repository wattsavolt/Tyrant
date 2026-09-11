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

		void Initialize() override;

		void Shutdown() override;

		void Update(float deltaTime) override;

		// Polled by the engine loop each frame
		bool WantsExit() const;

	private:
		AppBase* m_App;
	};
	
}