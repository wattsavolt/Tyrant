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
		// The game app the editor runs inside its own viewport for Play. Null when not in the editor.
		AppBase* m_EmbeddedApp = nullptr;
		// Loads the game app's library. Must outlive both apps above.
		LibraryLoader m_Loader;
	};

}