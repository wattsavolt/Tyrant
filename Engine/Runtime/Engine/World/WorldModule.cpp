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

	void WorldModule::Initialize()
	{
		m_WorldManager = new WorldManager();
	}

	void WorldModule::Update(float deltaTime)
	{
		m_WorldManager->Update(deltaTime);
	}

	void WorldModule::Shutdown()
	{
		delete m_WorldManager;
	}
}