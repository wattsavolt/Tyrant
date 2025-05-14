#include "Device.h"
#include "Sync.h"

namespace tyr
{
	Device::Device(uint index)
		: m_Index(index)
	{

	}

	Device::~Device()
	{

	}

	uint Device::GetQueueFamilyIndex(CommandQueueType type) const
	{
		return QUEUE_FAMILY_IGNORED;
	}
	
}