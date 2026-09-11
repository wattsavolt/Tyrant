#pragma once

#include "Base/Base.h"
#include "Module/IModule.h"
#include "EngineMacros.h"

namespace tyr
{
	class AssetManager;
	class TYR_ENGINE_API AssetModule final : public IModule
	{
	public:
		AssetModule();
		~AssetModule();

		void Initialize() override;

		void Shutdown() override;

		void Update(float deltaTime) override;

		const AssetManager* GetAssetManager() const { return m_AssetManager; }

		AssetManager* GetAssetManager() { return m_AssetManager; }

	private:
		AssetManager* m_AssetManager;
	};
	
}