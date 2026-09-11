#pragma once

#include "RendererMacros.h"
#include "Core.h"

namespace tyr
{
	struct BufferAllocation;

	struct GpuBufferAllocatorDesc
	{
		size_t totalSize;
	};

	class TYR_RENDERER_API GpuBufferAllocator final
	{
	public:
		GpuBufferAllocator(const GpuBufferAllocatorDesc& desc);

		bool Allocate(size_t size, size_t alignment, BufferAllocation& allocation);

		void Free(const BufferAllocation& allocation);

		void IncreaseCapacity(size_t newCapacity);

		size_t GetUsedSize() const { return m_UsedSize; }
		size_t GetFreeSize() const { return m_Capacity - m_UsedSize; }

	private:
		struct FreeBlock
		{
			size_t offset;
			size_t size;
		};

	private:
		void InsertFreeBlock(size_t offset, size_t size);
		void CombineFreeBlocks();

	private:
		Array<FreeBlock> m_FreeBlocks;

		size_t m_Capacity;
		size_t m_UsedSize = 0;
	};

}
