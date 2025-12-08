#include "AssetManager.h"
#include "BuildConfig.h"
#include "AssetRegistry.h"

namespace tyr
{
	AssetManager::AssetManager()
	{
		AssetRegistry::Instance().Load();
	}

	AssetManager::~AssetManager()
	{
		AssetRegistry::Instance().Save();
	}

	void AssetManager::Update(float deltaTime)
	{

	}
}