#include "AssetModule.h"
#include "AssetManager.h"

#include "BuildConfig.h"

namespace tyr
{
	AssetModule::AssetModule()
		: m_AssetManager(nullptr)
	{
		
	}

	AssetModule::~AssetModule()
	{
		
	}

	void AssetModule::InitializeModule()
	{
		m_AssetManager = new AssetManager();
	}

	void AssetModule::UpdateModule(float deltaTime)
	{
		m_AssetManager->Update(deltaTime);
	}

	void AssetModule::ShutdownModule()
	{
		delete m_AssetManager;
	}
}