#pragma once

#include "RenderAPI/AccelerationStructure.h"
#include "VulkanCommon.h"

namespace tyr
{
	struct AccelerationStructure
	{
		VkAccelerationStructureKHR accelerationStructure;
	};
}
