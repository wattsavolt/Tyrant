#pragma once

#include "Core.h"
#include "PhysicsMacros.h"
#include "Module/IModule.h"

namespace tyr
{
	class PhysicsManager;

	// Starts the physics library and holds its manager. Worlds step their own physics when they
	// update, so nothing runs here each frame.
	class TYR_PHYSICS_API PhysicsModule final : public IModule
	{
	public:
		PhysicsModule();
		~PhysicsModule();

		void Initialize() override;
		void Shutdown() override;
		void Update(float deltaTime) override;

		PhysicsManager* GetPhysicsManager() const { return m_PhysicsManager; }

	private:
		PhysicsManager* m_PhysicsManager{};
	};
}
