#include "WorldModule.h"
#include "WorldManager.h"

#include "BuildConfig.h"

namespace tyr
{
	WorldModule::WorldModule()
		: m_WorldManager(nullptr)
	{
		
	}

	WorldModule::~WorldModule()
	{
		
	}

	void WorldModule::InitializeModule()
	{
		m_WorldManager = new WorldManager();
	}

	void WorldModule::UpdateModule(float deltaTime)
	{
		m_WorldManager->Update(deltaTime);
	}

	void WorldModule::ShutdownModule()
	{
		delete m_WorldManager;
	}
}