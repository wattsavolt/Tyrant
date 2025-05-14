#include "EngineManager.h"
#include "App/AppBase.h"
#include "Engine.h" 

namespace tyr
{
	void EngineManager::RunEngine(const EngineParams& engineParams)
	{
		// TODO: Provide function that will load properties from file.
		EngineProperties properties;
		properties.maxTextures = 3000;
		Engine* engine = new Engine(properties);
		engine->Initialize(engineParams);
		engine->Run();
		engine->Shutdown();
		delete engine;
	}
}