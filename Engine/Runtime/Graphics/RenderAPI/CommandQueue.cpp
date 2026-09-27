
#include "CommandQueue.h"
#include "CommandList.h"
#include "Device.h"

namespace tyr
{
	CommandQueue::CommandQueue(Device& device, const GDebugString& debugName, CommandQueueType queueType)
		: m_Device(device)
		, m_DebugName(debugName)
		, m_QueueType(queueType)
	{
		SemaphoreDesc desc;
		desc.type = SemaphoreType::Timeline;
		desc.debugName = "QueueTimelineSemaphore";
		m_TimelineSemaphore = m_Device.CreateSemaphoreResource(desc);
	}

	CommandQueue::~CommandQueue()
	{
		m_Device.DeleteSemaphoreResource(m_TimelineSemaphore);
	}
}