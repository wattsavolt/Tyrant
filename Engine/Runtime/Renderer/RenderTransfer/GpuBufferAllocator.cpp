#include "GpuBufferAllocator.h"
#include "Core.h"
#include "RenderTransfer/BufferAllocation.h"

namespace tyr
{
	GpuBufferAllocator::GpuBufferAllocator(const GpuBufferAllocatorDesc& desc)
		: m_Capacity(desc.totalSize)
		, m_UsedSize(0)
	{
		m_FreeBlocks.Reserve(64);
		m_FreeBlocks.Add({ 0, desc.totalSize });
	}

	bool GpuBufferAllocator::Allocate(size_t size, size_t alignment, BufferAllocation& allocation)
	{
		for (uint i = 0; i < m_FreeBlocks.Size(); ++i)
		{
			FreeBlock& block = m_FreeBlocks[i];

			const size_t alignedOffset = MemoryUtil::Align(block.offset, alignment);
			const size_t padding = alignedOffset - block.offset;

			if (block.size < padding)
				continue;

			const size_t remaining = block.size - padding;

			if (remaining < size)
				continue;

			const size_t allocationEnd = alignedOffset + size;
			const size_t blockEnd = block.offset + block.size;

			// Create allocation
			allocation.offset = alignedOffset;
			allocation.size = size;

			// Update free block
			if (padding > 0 && allocationEnd < blockEnd)
			{
				// Split into two blocks
				size_t rightSize = blockEnd - allocationEnd;

				block.size = padding;

				m_FreeBlocks.Add({ allocationEnd, rightSize });
			}
			else if (padding > 0)
			{
				// Keep left side only
				block.size = padding;
			}
			else if (allocationEnd < blockEnd)
			{
				// Keep right side only
				block.offset = allocationEnd;
				block.size = blockEnd - allocationEnd;
			}
			else
			{
				// Fully consumed block
				m_FreeBlocks.Erase(i);
			}

			m_UsedSize += size;

			return true;
		}

		return false;
	}

	void GpuBufferAllocator::Free(const BufferAllocation& allocation)
	{
		if (allocation.size == 0)
			return;

		InsertFreeBlock(allocation.offset, allocation.size);

		m_UsedSize -= allocation.size;

		CombineFreeBlocks();
	}

	void GpuBufferAllocator::IncreaseCapacity(size_t newCapacity)
	{
		TYR_ASSERT(newCapacity > m_Capacity);

		const size_t oldCapacity = m_Capacity;
		const size_t additionalSize = newCapacity - m_Capacity;

		m_Capacity = newCapacity;

		InsertFreeBlock(oldCapacity, additionalSize);
	}

	void GpuBufferAllocator::InsertFreeBlock(size_t offset, size_t size)
	{
		uint insertIndex = 0;

		for (; insertIndex < m_FreeBlocks.Size(); ++insertIndex)
		{
			if (offset < m_FreeBlocks[insertIndex].offset)
				break;
		}

		m_FreeBlocks.Insert(insertIndex, { offset, size });
	}

	void GpuBufferAllocator::CombineFreeBlocks()
	{
		if (m_FreeBlocks.Size() < 2)
			return;

		for (uint i = 0; i < m_FreeBlocks.Size() - 1;)
		{
			FreeBlock& current = m_FreeBlocks[i];
			FreeBlock& next = m_FreeBlocks[i + 1];

			const size_t currentEnd = current.offset + current.size;

			if (currentEnd == next.offset)
			{
				current.size += next.size;
				m_FreeBlocks.Erase(i + 1);
			}
			else
			{
				++i;
			}
		}
	}
}