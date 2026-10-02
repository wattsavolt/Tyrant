#pragma once

#include "RendererMacros.h"
#include "Core.h"

namespace tyr
{
	struct FrameUploadAllocatorDesc
	{
		size_t totalSize; // total upload buffer size in bytes
		void* mappedBase = nullptr;
	};

	// Deliberately not thread-safe - every call (Allocate/Signal/Reclaim) must come from the
	// main thread. A worker thread needing to upload something must use its own dedicated
	// staging buffer instead.
	class TYR_RENDERER_API FrameUploadAllocator final
	{
	public:
		struct Allocation
		{
			void* cpuPtr;
			size_t offset;
			size_t size;
		};

		FrameUploadAllocator(const FrameUploadAllocatorDesc& desc);

		// Returns false if not enough space. Main thread only.
		bool Allocate(size_t size, size_t alignment, Allocation& allocation);

		// Snapshots the current head, tagged with frameNumber, right after a tick has finished
		// making all of its own Allocate() calls (once per tick, regardless of whether it
		// allocated anything). Main thread only.
		void RecordAllocationCheckpoint(uint64 frameNumber);

		// Call once per completion, with the frameNumber/signalValue it reported - never the
		// live head, since completions arrive asynchronously and the head may already reflect
		// later ticks' allocations by the time this runs. Main thread only.
		void Signal(uint64 frameNumber, uint64 signalValue);

		// Called once per submitted command list, per frame. Main thread only.
		void Reclaim(uint64 completedValue);

		uint64 GetSemaphoreValue() const { return m_SemaphoreValue; }

	private:

		struct SemaphorePoint
		{
			size_t offset;
			uint64 semaphoreValue;
		};

		// One entry per tick, in strict frame order - Signal() consumes these front-to-back,
		// since completions always arrive in the same order their ticks were recorded in.
		struct AllocationCheckpoint
		{
			size_t head;
			uint64 frameNumber;
		};

		FrameUploadAllocatorDesc m_Desc;

		size_t m_Head = 0; // write cursor
		size_t m_Tail = 0; // reclaim cursor

		uint64 m_SemaphoreValue; // current known value that has been completed

		Array<SemaphorePoint> m_SemaphorePoints;
		Array<AllocationCheckpoint> m_Checkpoints;
	};

}
