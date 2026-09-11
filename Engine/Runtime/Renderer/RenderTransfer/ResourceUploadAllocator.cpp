#include "ResourceUploadAllocator.h"
#include "Core.h"

namespace tyr
{
	ResourceUploadAllocator::ResourceUploadAllocator(const ResourceUploadAllocatorDesc& desc)
		: m_Desc(desc)
	{
		m_SemaphorePoints.Reserve(64);
	}

	bool ResourceUploadAllocator::HasSpace(size_t head, size_t tail, size_t size) const
	{
		if (head >= tail)
		{
			// free space is [head -> end] + [0 -> tail)
			const size_t spaceToEnd = m_Desc.totalSize - head;

			if (spaceToEnd >= size)
				return true;

			return size <= tail;
		}
		else
		{
			// free space is [head -> tail)
			return (tail - head) >= size;
		}
	}

	bool ResourceUploadAllocator::Allocate(size_t size, size_t alignment, Allocation& allocation)
	{
		size_t alignedHead = MemoryUtil::Align(m_Head, alignment);

		// Case 1: no wrap
		if (alignedHead + size <= m_Desc.totalSize)
		{
			if (m_Head >= m_Tail && alignedHead < m_Tail && (alignedHead + size) > m_Tail)
			{
				// Ensure we don't overlap tail
				return false;
			}

			allocation.offset = alignedHead;
			allocation.size = size;
			allocation.cpuPtr = static_cast<uint8*>(m_Desc.mappedBase) + alignedHead;

			m_Head = alignedHead + size;
			return true;
		}

		// Case 2: wrap to beginning
		alignedHead = MemoryUtil::Align(0ull, alignment);

		if (alignedHead + size > m_Tail)
			return false;

		allocation.offset = alignedHead;
		allocation.size = size;
		allocation.cpuPtr = static_cast<uint8*>(m_Desc.mappedBase) + alignedHead;

		m_Head = alignedHead + size;
		return true;
	}

	void ResourceUploadAllocator::Signal(uint64 signalValue)
	{
		m_SemaphorePoints.Add({ m_Head, signalValue });
	}

	void ResourceUploadAllocator::Reclaim(uint64 completedValue)
	{
		m_SemaphoreValue = completedValue;

		uint removeCount = 0;

		while (removeCount < m_SemaphorePoints.Size() && m_SemaphorePoints[removeCount].semaphoreValue <= completedValue)
		{
			m_Tail = m_SemaphorePoints[removeCount].offset;
			++removeCount;
		}

		if (removeCount > 0)
		{
			m_SemaphorePoints.EraseFromFront(removeCount);
		}
	}
}