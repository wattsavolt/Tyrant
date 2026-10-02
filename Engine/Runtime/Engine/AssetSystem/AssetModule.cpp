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
		// Safe here specifically because EngineLoop::Shutdown already called
		// TaskScheduler::WaitForAllTasks() before any module's Shutdown() ran - nothing can
		// still be pushing into AssetManager's pending queues by this point.
		m_AssetManager->FreePendingAssets();
		delete m_AssetManager;
	}
}