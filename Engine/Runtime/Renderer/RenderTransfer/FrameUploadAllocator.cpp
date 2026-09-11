#include "FrameUploadAllocator.h"
#include "Core.h"

namespace tyr
{
	FrameUploadAllocator::FrameUploadAllocator(const FrameUploadAllocatorDesc& desc)
		: m_Desc(desc)
		, m_SemaphoreValue(0)
	{
		m_SemaphorePoints.Reserve(32);
	}

	bool FrameUploadAllocator::Allocate(size_t size, size_t alignment, Allocation& allocation)
	{
		size_t alignedHead = MemoryUtil::Align(m_Head, alignment);
		size_t end = alignedHead + size;

		// Wrap case
		if (end > m_Desc.totalSize)
		{
			alignedHead = 0;
			end = size;

			// If wrapped region overlaps tail, then there's no space
			if (end > m_Tail)
				return false;

			m_Head = end;
		}
		else
		{
			// Normal overlap check
			if (alignedHead < m_Tail && end > m_Tail)
				return false;

			m_Head = end;
		}

		allocation.cpuPtr = static_cast<void*>(static_cast<uint8*>(m_Desc.mappedBase) + alignedHead);
		allocation.offset = alignedHead;
		allocation.size = size;

		return true;
	}

	void FrameUploadAllocator::Signal(uint64 signalValue)
	{
		m_SemaphorePoints.Add({ m_Head, signalValue });
	}

	void FrameUploadAllocator::Reclaim(uint64 completedValue)
	{
		m_SemaphoreValue = completedValue;

		uint removeCount = 0;

		while (removeCount < m_SemaphorePoints.Size() && m_SemaphorePoints[removeCount].semaphoreValue <= m_SemaphoreValue)
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