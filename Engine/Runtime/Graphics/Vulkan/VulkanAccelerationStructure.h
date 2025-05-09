#pragma once

#include "RenderAPI/AccelerationStructure.h"
#include "VulkanDevice.h"

namespace tyr
{
	// TODO: Implement
	class VulkanAccelerationStructure final : public AccelerationStructure
	{
	public:
		VulkanAccelerationStructure(VulkanDevice& device, const AccelerationStructureDesc& desc);
		~VulkanAccelerationStructure();
		
	private:
		VulkanDevice& m_Device;
	};
}
