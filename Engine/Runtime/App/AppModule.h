#pragma once

#include "Core.h"
#include "Utility/LibraryLoader.h"

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
		// Only actually loads anything when m_App is created via the DLL below, not when it's
		// an in-process Editor - Unload() (called from ~LibraryLoader) is a safe no-op either way.
		LibraryLoader m_Loader;
	};

}