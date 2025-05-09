
#include "Sync.h"

namespace tyr
{
	Fence::Fence(const FenceDesc& desc)
		: m_Desc(desc)
	{

	}
	
	Semaphore::Semaphore(const SemaphoreDesc& desc)
		: m_Desc(desc)
	{

	}

	Event::Event(const EventDesc& desc)
		: m_Desc(desc)
	{

	}
}