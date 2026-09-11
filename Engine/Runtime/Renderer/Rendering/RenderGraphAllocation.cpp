#include "RenderGraphAllocation.h"
#include "Memory/ScratchAllocator.h"

namespace tyr
{
	ScratchAllocatorPool<RenderGraphAllocator::c_AllocatorCount>* RenderGraphAllocator::s_ScratchAllocatorPool = nullptr;
	bool RenderGraphAllocator::s_Initialized = false;

	void RenderGraphAllocator::Create(size_t blockSize)
	{
		TYR_ASSERT(!s_Initialized);
		s_ScratchAllocatorPool = new ScratchAllocatorPool<c_AllocatorCount>(blockSize);
	}

	uint8* RenderGraphAllocator::Alloc(size_t size)
	{
		TYR_ASSERT(s_Initialized);
		return s_ScratchAllocatorPool->Alloc(size);
	}

	uint8* RenderGraphAllocator::AllocAligned(size_t size, size_t alignment)
	{
		TYR_ASSERT(s_Initialized);
		return s_ScratchAllocatorPool->AllocAligned(size, alignment);
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