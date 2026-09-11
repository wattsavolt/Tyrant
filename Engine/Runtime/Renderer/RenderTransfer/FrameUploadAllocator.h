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

		// Returns false if not enough space
		bool Allocate(size_t size, size_t alignment, Allocation& allocation);

		// Call once per submission
		void Signal(uint64 signalValue);

		// Should be called once submission of commandlist per frame
		void Reclaim(uint64 completedValue);

		uint64 GetSemaphoreValue() const { return m_SemaphoreValue; }

	private:

		struct SemaphorePoint
		{
			size_t offset;
			uint64 semaphoreValue;
		};

		FrameUploadAllocatorDesc m_Desc;

		size_t m_Head = 0; // write cursor
		size_t m_Tail = 0; // reclaim cursor

		uint64 m_SemaphoreValue; // current known value that has been completed

		Array<SemaphorePoint> m_SemaphorePoints;
	};

}
