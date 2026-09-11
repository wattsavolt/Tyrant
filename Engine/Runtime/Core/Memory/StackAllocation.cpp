#include "StackAllocation.h"

namespace tyr
{
	MemoryStack& MemoryStack::Instance()
	{
		static TYR_THREADLOCAL MemoryStack memoryStack;
		return memoryStack;
	}

	uint8* MemoryStack::Alloc(size_t numBytes)
	{
		return m_Stack.Allocate(static_cast<uint>(numBytes));
	}

	void MemoryStack::DeallocLast()
	{
		m_Stack.Deallocate();
	}

	StackAllocManager::StackAllocManager()
		: m_Allocs(0u)
	{
	}

	StackAllocManager::~StackAllocManager()
	{
		for (uint64 i = 0; i < m_Allocs; ++i)
		{
			StackFreeLast();
		}
	}
}