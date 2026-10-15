#pragma once

#include "PhysicsMacros.h"
#include "PhysicsTypes.h"

namespace tyr
{
	struct PhysicsContextData;

	// Starts the physics library: its memory, shape types and worker jobs. Made once, before any
	// shape or scene, and destroyed after them.
	class TYR_PHYSICS_API PhysicsContext final : INonCopyable
	{
	public:
		PhysicsContext();
		~PhysicsContext();

		PhysicsContextData& GetData() const { return *m_Data; }

	private:
		PhysicsContextData* m_Data = nullptr;
	};
}
