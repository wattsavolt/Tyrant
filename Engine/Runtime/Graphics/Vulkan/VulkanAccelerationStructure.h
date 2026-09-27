#pragma once

#include "RenderAPI/AccelerationStructure.h"
#include "VulkanCommon.h"

namespace tyr
{
	struct AccelerationStructure
	{
		VkAccelerationStructureKHR accelerationStructure;
		BufferHandle backingBuffer;
		VkDeviceAddress deviceAddress;
		VkDeviceSize buildScratchSize;
		VkDeviceSize updateScratchSize;
		// Kept so BuildAccelerationStructures can rebuild the VkAccelerationStructureGeometryKHR
		// info at actual build time (with real buffer addresses, unlike the size-query-only info
		// used at creation) without the caller having to resupply it.
		AccelerationStructureDesc desc;
	};
}
