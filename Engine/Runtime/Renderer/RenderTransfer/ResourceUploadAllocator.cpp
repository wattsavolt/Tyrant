#include "ResourceUploadAllocator.h"
#include "Core.h"

namespace tyr
{
	ResourceUploadAllocator::ResourceUploadAllocator(const ResourceUploadAllocatorDesc& desc)
		: m_Desc(desc)
	{
		m_ChunkCount = (uint)(desc.totalSize / c_ChunkSize);
		TYR_ASSERT(m_ChunkCount > 0 && m_ChunkCount <= c_MaxChunks);

		for (uint i = 0; i < m_ChunkCount; ++i)
		{
			m_ChunkFree[i] = true;
		}
	}

	bool ResourceUploadAllocator::Allocate(size_t size, size_t alignment, Allocation& allocation)
	{
		TYR_ASSERT(alignment <= c_ChunkSize);

		const uint chunksNeeded = (uint)((size + c_ChunkSize - 1) / c_ChunkSize);

		uint runStart = 0;
		uint runLength = 0;
		for (uint i = 0; i < m_ChunkCount; ++i)
		{
			if (m_ChunkFree[i])
			{
				if (runLength == 0)
				{
					runStart = i;
				}
				++runLength;

				if (runLength == chunksNeeded)
				{
					break;
				}
			}
			else
			{
				runLength = 0;
			}
		}

		if (runLength < chunksNeeded)
		{
			return false;
		}

		for (uint i = runStart; i < runStart + chunksNeeded; ++i)
		{
			m_ChunkFree[i] = false;
		}

		const Handle id = m_InFlightPool.Create();
		InFlightAllocation& record = m_InFlightPool[id];
		record.startChunk = runStart;
		record.chunkCount = chunksNeeded;
		record.signalled = false;
		record.fenceValue = 0;

		m_LiveIds.Add(id);

		allocation.offset = (size_t)runStart * c_ChunkSize;
		allocation.size = size;
		allocation.cpuPtr = static_cast<uint8*>(m_Desc.mappedBase) + allocation.offset;
		allocation.id = id;

		return true;
	}

	void ResourceUploadAllocator::Signal(Handle id, uint64 signalValue)
	{
		InFlightAllocation& record = m_InFlightPool[id];
		record.signalled = true;
		record.fenceValue = signalValue;
	}

	void ResourceUploadAllocator::Reclaim(uint64 completedValue)
	{
		for (uint i = 0; i < m_LiveIds.Size();)
		{
			const Handle id = m_LiveIds[i];
			InFlightAllocation& record = m_InFlightPool[id];

			if (record.signalled && record.fenceValue <= completedValue)
			{
				for (uint c = record.startChunk; c < record.startChunk + record.chunkCount; ++c)
				{
					m_ChunkFree[c] = true;
				}

				m_InFlightPool.Delete(id);
				m_LiveIds.SwapAndPopBack(i);
			}
			else
			{
				++i;
			}
		}
	}
}
