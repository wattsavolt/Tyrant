#include "PhysicsModule.h"
#include "PhysicsManager.h"

namespace tyr
{
	PhysicsModule::PhysicsModule()
	{
	}

	PhysicsModule::~PhysicsModule()
	{
	}

	void PhysicsModule::Initialize()
	{
		m_PhysicsManager = new PhysicsManager();
	}

	void PhysicsModule::Update(float /*deltaTime*/)
	{
	}

	void PhysicsModule::Shutdown()
	{
		TYR_SAFE_DELETE(m_PhysicsManager);
	}
}
