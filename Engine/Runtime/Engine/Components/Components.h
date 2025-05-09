#pragma once

#include "EngineMacros.h"
#include "Math/Quaternion.h"
#include "Math/Vector3.h"

namespace tyr
{
	constexpr uint MaxChildComponents = 4u;

	struct Transform
	{
		Vector3 scale;
		Quaternion rotation;
		Vector3 position;
	};

	struct ComponentTransform
	{
		Transform local;
		Transform world;
		int parentIndex = -1;
	};
	

}