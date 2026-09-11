#include "Memory/FrameAllocation.h"
#include "ScratchAllocator.h"

namespace tyr
{
	TYR_THREADLOCAL ScratchAllocator* FrameAllocator::s_Allocator = nullptr;

	void FrameAllocator::Create(size_t blockSize)
	{
		if (s_Allocator != nullptr)
		{
			Destroy();
		}

		s_Allocator = new ScratchAllocator(blockSize);
	}

	uint8* FrameAllocator::Alloc(size_t size)
	{
		TYR_ASSERT(s_Allocator != nullptr);

		return s_Allocator->Alloc(size);
	}

	uint8* FrameAllocator::AllocAligned(size_t size, size_t alignment)
	{
		TYR_ASSERT(s_Allocator != nullptr);

		return s_Allocator->AllocAligned(size, alignment);
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