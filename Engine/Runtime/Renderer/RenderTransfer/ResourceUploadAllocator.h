#pragma once

#include "RendererMacros.h"
#include "Core.h"
#include "RenderAPI/GraphicsBase.h"
#include "RenderAPI/Sync.h"
#include "Memory/PoolHandle.h"
#include "Memory/LocalObjectPool.h"

namespace tyr
{
	struct ResourceUploadAllocatorDesc
	{
		size_t totalSize;     // total upload buffer size
		void* mappedBase;    // persistently mapped CPU pointer
	};

	// Allocates fixed-size chunks out of a shared upload buffer for resources that are
	// loaded in the background and submitted whenever they're ready, not necessarily in
	// the order they were allocated. Each allocation tracks its own completion separately,
	// so one slow resource never blocks reclaiming a different, faster one.
	class TYR_RENDERER_API ResourceUploadAllocator final
	{
	public:
		struct Allocation
		{
			void* cpuPtr;
			size_t offset;
			size_t size;
			Handle id;
		};

		static constexpr size_t c_ChunkSize = 2 * 1024 * 1024;
		static constexpr uint c_MaxChunks = 512;
		static constexpr uint c_MaxInFlightAllocations = 256;

	public:
		ResourceUploadAllocator(const ResourceUploadAllocatorDesc& desc);

		// Returns false if not enough contiguous free chunks
		bool Allocate(size_t size, size_t alignment, Allocation& allocation);

		// Marks an allocation as submitted, along with the timeline value the GPU will
		// have reached once it's actually finished with it. Only ever called from the
		// main thread.
		void Signal(Handle id, uint64 signalValue);

		// Frees every allocation that's been signalled and whose value has now completed.
		// Only ever called from the main thread.
		void Reclaim(uint64 completedValue);

		// Unconditionally frees every still-live allocation, signalled or not. Only safe once the
		// caller has independently confirmed the GPU is fully idle - every allocation this pool
		// still knows about has necessarily already been consumed by then. Main thread only, at shutdown.
		void ReclaimAll();

	private:
		struct InFlightAllocation
		{
			uint startChunk = 0;
			uint chunkCount = 0;
			bool signalled = false;
			uint64 fenceValue = 0;
		};

	private:
		ResourceUploadAllocatorDesc m_Desc;

		uint m_ChunkCount = 0;
		bool m_ChunkFree[c_MaxChunks];

		LocalObjectPool<InFlightAllocation, c_MaxInFlightAllocations> m_InFlightPool;
		LocalArray<Handle, c_MaxInFlightAllocations> m_LiveIds;
	};
}
