#include "VulkanAccelerationStructure.h"
#include "VulkanDevice.h"

namespace tyr
{
	AccelerationStructureHandle Device::CreateAccelerationStructure(const AccelerationStructureDesc& desc)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		const AccelerationStructureHandle handle(device.m_AccelerationStructurePool.Create());
		AccelerationStructure& accelerationStructure = device.m_AccelerationStructurePool[handle.h];
		// TODO: Implement
		return handle;
	}

	void Device::DeleteAccelerationStructure(AccelerationStructureHandle handle)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		AccelerationStructure& accelerationStructure = device.GetAccelerationStructure(handle);
		// TODO: Implement
		device.m_AccelerationStructurePool.Delete(handle.h);
	}
}