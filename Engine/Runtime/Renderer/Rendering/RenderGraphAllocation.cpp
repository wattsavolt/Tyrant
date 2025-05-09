#include "RenderGraphAllocation.h"
#include "Memory/ScratchAllocator.h"

namespace tyr
{
	ScratchAllocator* RenderGraphAllocator::s_Allocator = nullptr;

	void RenderGraphAllocator::Create(uint blockSize)
	{
		if (s_Allocator != nullptr)
		{
			Destroy();
		}

		s_Allocator = new ScratchAllocator(blockSize);
	}

	uint8* RenderGraphAllocator::Alloc(uint amount)
	{
		TYR_ASSERT(s_Allocator != nullptr);

		return s_Allocator->Alloc(amount);
	}

	uint8* RenderGraphAllocator::AllocAligned(uint amount, uint alignment)
	{
		TYR_ASSERT(s_Allocator != nullptr);

		return s_Allocator->AllocAligned(amount, alignment);
	}

	void RenderGraphAllocator::Reset()
	{
		TYR_ASSERT(s_Allocator != nullptr);

		s_Allocator->Reset();
	}

	void RenderGraphAllocator::Destroy()
	{
		TYR_ASSERT(s_Allocator != nullptr);

		TYR_SAFE_DELETE(s_Allocator);
	}
}