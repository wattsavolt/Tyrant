#include "Memory/FrameAllocation.h"
#include "ScratchAllocator.h"

namespace tyr
{
	TYR_THREADLOCAL ScratchAllocator* FrameAllocator::s_Allocator = nullptr;

	void FrameAllocator::Create(uint blockSize)
	{
		if (s_Allocator != nullptr)
		{
			Destroy();
		}

		s_Allocator = new ScratchAllocator(blockSize);
	}

	uint8* FrameAllocator::Alloc(uint amount)
	{
		TYR_ASSERT(s_Allocator != nullptr);

		return s_Allocator->Alloc(amount);
	}

	uint8* FrameAllocator::AllocAligned(uint amount, uint alignment)
	{
		TYR_ASSERT(s_Allocator != nullptr);

		return s_Allocator->AllocAligned(amount, alignment);
	}

	void FrameAllocator::Reset()
	{
		TYR_ASSERT(s_Allocator != nullptr);

		s_Allocator->Reset();
	}

	void FrameAllocator::Destroy()
	{
		TYR_ASSERT(s_Allocator != nullptr);

		TYR_SAFE_DELETE(s_Allocator);
	}
}