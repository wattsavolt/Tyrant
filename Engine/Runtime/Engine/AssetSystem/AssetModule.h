#pragma once

#include "Base/Base.h"
#include "Module/IModule.h"
#include "EngineMacros.h"

namespace tyr
{
	class AssetManager;
	class TYR_ENGINE_EXPORT AssetModule final : public IModule
	{
	public:
		AssetModule();
		~AssetModule();

		void InitializeModule() override;

		void ShutdownModule() override;

		void UpdateModule(float deltaTime) override;

		const AssetManager* GetAssetManager() const { return m_AssetManager; }

		AssetManager* GetAssetManager() { return m_AssetManager; }

	private:
		AssetManager* m_AssetManager;
	};
	
}