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

	void AssetModule::Initialize()
	{
		m_AssetManager = new AssetManager();
	}

	void AssetModule::Update(float deltaTime)
	{
		m_AssetManager->Update(deltaTime);
	}

	void AssetModule::Shutdown()
	{
		delete m_AssetManager;
	}
}