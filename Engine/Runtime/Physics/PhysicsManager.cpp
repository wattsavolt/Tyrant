#include "PhysicsManager.h"
#include "PhysicsContext.h"
#include "PhysicsScene.h"

namespace tyr
{
	PhysicsManager::PhysicsManager()
	{
		m_Context = new PhysicsContext();
	}

	PhysicsManager::~PhysicsManager()
	{
		delete m_Context;
	}

	Handle PhysicsManager::CreatePhysicsWorld()
	{
		const Handle handle = m_Worlds.Create();
		PhysicsWorldEntry& entry = m_Worlds[handle];
		// Made when a game world starts and kept until it ends.
		entry.scene = new PhysicsScene(*m_Context);
		entry.accumulator = 0.0f;
		return handle;
	}

	void PhysicsManager::DestroyPhysicsWorld(Handle physicsWorld)
	{
		delete m_Worlds[physicsWorld].scene;
		m_Worlds.Delete(physicsWorld);
	}

	PhysicsScene& PhysicsManager::GetScene(Handle physicsWorld)
	{
		return *m_Worlds[physicsWorld].scene;
	}

	const PhysicsScene& PhysicsManager::GetScene(Handle physicsWorld) const
	{
		return *m_Worlds[physicsWorld].scene;
	}

	uint PhysicsManager::AdvanceTime(Handle physicsWorld, float deltaTime)
	{
		PhysicsWorldEntry& entry = m_Worlds[physicsWorld];
		entry.accumulator += deltaTime;
		uint steps = 0;
		while (entry.accumulator >= PhysicsConstants::c_FixedTimeStep && steps < PhysicsConstants::c_MaxStepsPerFrame)
		{
			entry.accumulator -= PhysicsConstants::c_FixedTimeStep;
			++steps;
		}
		if (entry.accumulator >= PhysicsConstants::c_FixedTimeStep)
		{
			entry.accumulator = 0.0f;
		}
		return steps;
	}

	float PhysicsManager::GetInterpolation(Handle physicsWorld) const
	{
		return m_Worlds[physicsWorld].accumulator / PhysicsConstants::c_FixedTimeStep;
	}
}
