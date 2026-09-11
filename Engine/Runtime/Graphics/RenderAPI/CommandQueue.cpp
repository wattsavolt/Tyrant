
#include "CommandQueue.h"
#include "CommandList.h"

namespace tyr
{
	CommandQueue::CommandQueue(const GDebugString& debugName, CommandQueueType queueType)
		: m_DebugName(debugName)
		, m_QueueType(queueType)
	{

	}
}