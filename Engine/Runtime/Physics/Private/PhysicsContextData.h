#pragma once

#include "Core.h"

namespace JPH
{
	class TempAllocatorImpl;
}

namespace tyr
{
	class PhysicsJobSystem;

	// What every scene shares: scratch memory for each step and the jobs it runs on.
	struct PhysicsContextData
	{
		JPH::TempAllocatorImpl* tempAllocator = nullptr;
		PhysicsJobSystem* jobSystem = nullptr;
	};
}
