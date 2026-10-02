#pragma once

#include "RenderAPI/AccelerationStructure.h"
#include "VulkanCommon.h"

namespace tyr
{
	struct AccelerationStructure
	{
		VkAccelerationStructureKHR accelerationStructure;
		BufferHandle backingBuffer;
		// True when backingBuffer was supplied by the caller (CreateAccelerationStructureAt,
		// for a suballocated shared storage buffer) rather than created internally for this
		// structure alone - DeleteAccelerationStructure must not free a buffer it doesn't own.
		bool externalBackingBuffer = false;
		VkDeviceAddress deviceAddress;
		VkDeviceSize buildScratchSize;
		VkDeviceSize updateScratchSize;
		// Kept so BuildAccelerationStructures can rebuild the VkAccelerationStructureGeometryKHR
		// info at actual build time (with real buffer addresses, unlike the size-query-only info
		// used at creation) without the caller having to resupply it.
		AccelerationStructureDesc desc;
	};
}
