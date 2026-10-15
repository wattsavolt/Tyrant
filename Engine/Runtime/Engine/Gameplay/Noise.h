#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "Math/Vector3.h"
#include "ECS/EntitySystem.h"

namespace tyr
{
	struct World;

	namespace Noise
	{
		// How long a noise stays in the world for anything to hear it, in seconds.
		constexpr float c_Lifetime = 1.0f;

		// Adds a noise heard out to radius metres. When there are already as many as fit, it
		// replaces the oldest.
		TYR_ENGINE_API void Emit(World& world, const Vector3& position, float radius, Entity source);

		// Ages the world's noises and drops the ones past their lifetime.
		TYR_ENGINE_API void Update(World& world, float deltaTime);
	}
}
