#pragma once

#include "Core.h"
#include "Math/Vector3.h"
#include "ECS/EntitySystem.h"

namespace tyr
{
	// What a ray hit, as the entity whose collider it was.
	struct RayCastEntityHit
	{
		Entity entity = c_InvalidEntity;
		Vector3 position;
		Vector3 normal;
		float distance = 0.0f;
	};
}
