#pragma once

#include "Base/Base.h"
#include "Module/IModule.h"
#include "EngineMacros.h"

namespace tyr
{
	class WorldManager;
	class TYR_ENGINE_EXPORT WorldModule final : public IModule
	{
	public:
		WorldModule();
		~WorldModule();

		void InitializeModule() override;

		void ShutdownModule() override;

		void UpdateModule(float deltaTime) override;

		const WorldManager* GetWorldManager() const { return m_WorldManager; }

		WorldManager* GetWorldManager() { return m_WorldManager; }

	private:
		WorldManager* m_WorldManager;
	};
	
}