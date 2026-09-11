#pragma once

#include "RendererMacros.h"
#include "Core.h"
#include "RenderAPI/GraphicsBase.h"
#include "RenderAPI/Sync.h"

namespace tyr
{
	struct ResourceUploadAllocatorDesc
	{
		size_t totalSize;     // total upload buffer size
		void* mappedBase;    // persistently mapped CPU pointer
	};

	class TYR_RENDERER_API ResourceUploadAllocator final
	{
	public:

		struct Allocation
		{
			void* cpuPtr;
			size_t offset;
			size_t size;
		};

	public:
		ResourceUploadAllocator(const ResourceUploadAllocatorDesc& desc);

		// Returns false if not enough space
		bool Allocate(size_t size, size_t alignment, Allocation& allocation);

		// Call once per submission of resource transfers 
		void Signal(uint64 signalValue);

		// Should be called once per frame
		void Reclaim(uint64 completedValue);

		uint64 GetSemaphoreValue() const { return m_SemaphoreValue; }

	private:
		struct SemaphorePoint
		{
			size_t offset;
			uint64 semaphoreValue;
		};

	private:
		bool HasSpace(size_t head, size_t tail, size_t size) const;

	private:

		ResourceUploadAllocatorDesc m_Desc;

		size_t m_Head = 0; // write cursor
		size_t m_Tail = 0; // reclaim cursor

		uint64 m_SemaphoreValue = 0;

		Array<SemaphorePoint> m_SemaphorePoints;
	};
}
