#include "VulkanAccelerationStructure.h"
#include "VulkanDevice.h"

namespace tyr
{
	VulkanAccelerationStructure::VulkanAccelerationStructure(VulkanDevice& device, const AccelerationStructureDesc& desc)
		: AccelerationStructure(desc)
		, m_Device(device)
	{
		
	}

	VulkanAccelerationStructure::~VulkanAccelerationStructure()
	{
		
	}
}