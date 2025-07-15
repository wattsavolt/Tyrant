#pragma once

#include "Allocation.h"
#include "Utility/Utility.h"
#include "Containers/Array.h"

namespace tyr
{
	/// Allocates blocks of memory as required and all memory is freed at once. 
	class TYR_CORE_EXPORT ScratchAllocator final
	{
	private:
		/// A single block of memory of BlockSize size. A pointer to the first free address is stored, and a remaining
		/// size.
		class Block final
		{
		public:
			Block(uint size)
				: m_Size(size) { }

			~Block() = default;

			/// Returns the first free address and increments the free pointer. Caller needs to ensure the remaining block
			/// size is adequate before calling.
			uint8* Alloc(uint amount)
			{
				uint8* ptr = &m_Data[m_Pos];
				m_Pos += amount;
				return ptr;
			}

			/// Doesn't free the memory but clears the allocations
			void Clear()
			{
				m_Pos = 0;
			}

			uint8* m_Data = nullptr;
			// Pointer to current memory location
			uint m_Pos = 0;
			uint m_Size = 0;
		};

	public:
		ScratchAllocator(uint blockSize = 1024 * 1024);
		~ScratchAllocator();

		/// Allocates memory of the size provided using 16 byte alignment.
		uint8* Alloc(uint amount);

		/// Allocates memory of the size provided with the specified alignment as the boundary.
		/// @note The alignment must be a power of 2
		uint8* AllocAligned(uint amount, uint alignment);

		/// Clears all allocations, combines all blocks into one and starts a new period from scratch.
		void Reset();

		uint GetTotalSize() const;

	private:
		/// Allocates a new block of memory of the specified size.
		void AllocBlock(uint amount);

		/// Deallocates the block's memory
		void FreeBlock(Block* block);

		uint m_MinBlockSize;
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
		uint8* Alloc(uint amount)
		{
			return m_Allocators[m_AllocatorIndex]->Alloc(amount);
		}

		/// @see ScratchAllocator AllocAligned
		uint8* AllocAligned(uint amount, uint alignment)
		{
			return m_Allocators[m_AllocatorIndex]->AllocAligned(amount, alignment);
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



