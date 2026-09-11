#include "TempAllocation.h"
#include "BlockAllocator.h"

namespace tyr
{
	TempAllocator& TempAllocator::Instance()
	{
		static TYR_THREADLOCAL TempAllocator allocator;
		return allocator;
	}

	TempAllocator::TempAllocator()
		: m_Allocator(new BlockAllocator(1024 * 1024))
	{

	}

	TempAllocator::~TempAllocator()
	{
		delete m_Allocator;
	}

	uint8* TempAllocator::Alloc(size_t numBytes)
	{
		return m_Allocator->Alloc(numBytes);
	}

	uint8* TempAllocator::AllocAligned(size_t size, size_t alignment)
	{
		return m_Allocator->AllocAligned(size, alignment);
	}

	void TempAllocator::Free(uint8* ptr)
	{
		m_Allocator->Free(ptr);
	}

	void TempAllocator::FreeAligned(uint8* ptr)
	{
		m_Allocator->Free(ptr);
	}
}