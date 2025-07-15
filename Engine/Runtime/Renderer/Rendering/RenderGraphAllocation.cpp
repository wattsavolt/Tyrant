#include "RenderGraphAllocation.h"
#include "Memory/ScratchAllocator.h"

namespace tyr
{
	ScratchAllocatorPool<RenderGraphAllocator::c_AllocatorCount>* RenderGraphAllocator::s_ScratchAllocatorPool = nullptr;
	bool RenderGraphAllocator::s_Initialized = false;

	void RenderGraphAllocator::Create(uint blockSize)
	{
		TYR_ASSERT(!s_Initialized);
		s_ScratchAllocatorPool = new ScratchAllocatorPool<c_AllocatorCount>(blockSize);
	}

	uint8* RenderGraphAllocator::Alloc(uint amount)
	{
		TYR_ASSERT(s_Initialized);
		return s_ScratchAllocatorPool->Alloc(amount);
	}

	uint8* RenderGraphAllocator::AllocAligned(uint amount, uint alignment)
	{
		TYR_ASSERT(s_Initialized);
		return s_ScratchAllocatorPool->AllocAligned(amount, alignment);
	}

	void RenderGraphAllocator::NextFrame()
	{
		TYR_ASSERT(s_Initialized);
		s_ScratchAllocatorPool->Next();
	}

	void RenderGraphAllocator::Destroy()
	{
		TYR_ASSERT(s_Initialized);
		TYR_SAFE_DELETE(s_ScratchAllocatorPool);
		s_Initialized = false;
	}
}