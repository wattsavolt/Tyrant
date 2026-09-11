#pragma once

#include "Allocation.h"
#include "Utility/Utility.h"
#include "Containers/Array.h"

namespace tyr
{
	/// Allocates blocks of memory as required and all memory is freed at once. 
	class TYR_CORE_API ScratchAllocator final
	{
	private:
		/// A single block of memory of BlockSize size. A pointer to the first free address is stored, and a remaining
		/// size.
		class Block final
		{
		public:
			Block(size_t size)
				: m_Size(size) { }

			~Block() = default;

			/// Returns the first free address and increments the free pointer. Caller needs to ensure the remaining block
			/// size is adequate before calling.
			uint8* Alloc(size_t size)
			{
				uint8* ptr = &m_Data[m_Pos];
				m_Pos += size;
				return ptr;
			}

			/// Doesn't free the memory but clears the allocations
			void Clear()
			{
				m_Pos = 0;
			}

			uint8* m_Data = nullptr;
			// Pointer to current memory location
			size_t m_Pos = 0;
			size_t m_Size = 0;
		};

	public:
		ScratchAllocator(size_t blockSize = 1024 * 1024);
		~ScratchAllocator();

		/// Allocates memory of the size provided using 16 byte alignment.
		uint8* Alloc(size_t size);

		/// Allocates memory of the size provided with the specified alignment as the boundary.
		/// @note The alignment must be a power of 2
		uint8* AllocAligned(size_t size, size_t alignment);

		/// Clears all allocations, combines all blocks into one and starts a new period from scratch.
		void Reset();

		size_t GetTotalSize() const;

	private:
		/// Allocates a new block of memory of the specified size.
		void AllocBlock(size_t size);

		/// Deallocates the block's memory
		void FreeBlock(Block* block);

		size_t m_MinBlockSize;
		Array<Block*> m_Blocks;
		Block* m_CurBlock;
	};

	template <uint N>
	class ScratchAllocatorPool final
	{
	public:
		ScratchAllocatorPool(uint blockSize = 1024 * 1024)
			: m_AllocatorIndex(0) 
		{ 
			for (uint i = 0; i < N; ++i)
			{
				m_Allocators[i] = new ScratchAllocator(blockSize);
			}
		}

		~ScratchAllocatorPool()
		{
			for (uint i = 0; i < N; ++i)
			{
				TYR_SAFE_DELETE(m_Allocators[i]);
			}
		}

		/// @see ScratchAllocator Alloc
		uint8* Alloc(size_t size)
		{
			return m_Allocators[m_AllocatorIndex]->Alloc(size);
		}

		/// @see ScratchAllocator AllocAligned
		uint8* AllocAligned(size_t size, size_t alignment)
		{
			return m_Allocators[m_AllocatorIndex]->AllocAligned(size, alignment);
		}

		void Next()
		{
			m_AllocatorIndex = (m_AllocatorIndex + 1) % N;
			// Only reset before next use to keep the data around for N number of uses
			m_Allocators[m_AllocatorIndex]->Reset();
		}

	private:
		ScratchAllocator* m_Allocators[N];
		uint m_AllocatorIndex;
	};
}



