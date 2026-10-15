#pragma once

#include "Core.h"
#include "NavmeshMacros.h"
#include "Module/IModule.h"

namespace tyr
{
	class NavmeshManager;

	// Starts the navmesh library and holds its manager. Nothing runs here each frame.
	class TYR_NAVMESH_API NavmeshModule final : public IModule
	{
	public:
		NavmeshModule();
		~NavmeshModule();

		void Initialize() override;
		void Shutdown() override;
		void Update(float deltaTime) override;

		NavmeshManager* GetNavmeshManager() const { return m_NavmeshManager; }

	private:
		NavmeshManager* m_NavmeshManager{};
	};
}
