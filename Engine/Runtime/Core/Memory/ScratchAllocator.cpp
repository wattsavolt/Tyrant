#include "ScratchAllocator.h"

namespace tyr
{
	ScratchAllocator::ScratchAllocator(size_t blockSize)
		: m_MinBlockSize(blockSize)
		, m_CurBlock(nullptr)
	{
		TYR_ASSERT(blockSize > 0);
	}

	ScratchAllocator::~ScratchAllocator()
	{
		const uint numBlocks = m_Blocks.Size();
		for (uint i = 0; i < numBlocks; ++i)
		{
			FreeBlock(m_Blocks[i]);
		}
	}

	uint8* ScratchAllocator::Alloc(size_t size)
	{
		TYR_ASSERT(size != 0);

		uint freeMem = 0;
		if (m_CurBlock)
		{
			freeMem = m_CurBlock->m_Size - m_CurBlock->m_Pos;
		}

		if (size > freeMem)
		{
			AllocBlock(size);
		}

		return m_CurBlock->Alloc(size);
	}

	uint8* ScratchAllocator::AllocAligned(size_t size, size_t alignment)
	{
		size_t freeMem = 0;
		size_t pos = 0;

		if (m_CurBlock)
		{
			freeMem = m_CurBlock->m_Size - m_CurBlock->m_Pos;
			pos = m_CurBlock->m_Pos;
		}

		size_t alignOffset = (alignment - (pos & (alignment - 1))) & (alignment - 1);
		if (size + alignOffset > freeMem)
		{
			// Allocate new block with 16 byte alignment
			if (alignment > 16)
			{
				alignOffset = alignment - 16;
			}
			else
			{
				alignOffset = 0;
			}

			AllocBlock(size + alignOffset);
		}

		size += alignOffset;

		uint8* data = m_CurBlock->Alloc(size);
		// The data will begin after the offset as it may be padded to have 16 byte alignment
		return data + alignOffset;
	}

	void ScratchAllocator::AllocBlock(size_t size)
	{
		size_t blockSize = m_MinBlockSize;
		if (size > blockSize)
		{
			blockSize = size;
		}

		const size_t alignOffset = 16 - (sizeof(Block) & (16 - 1));

		uint8* data = reinterpret_cast<uint8*>(MemAllocAligned16(blockSize + sizeof(Block) + alignOffset));
		Block* block = new (data) Block(blockSize);
		data += sizeof(Block) + alignOffset;
		block->m_Data = data;

		m_Blocks.Add(block);

		m_CurBlock = block;
	}

	void ScratchAllocator::FreeBlock(Block* block)
	{
		block->~Block();
		FreeAligned16(block);
	}

	void ScratchAllocator::Reset()
	{
		if (m_CurBlock)
		{
			const uint numBlocks = m_Blocks.Size();
			if (numBlocks > 1)
			{
				size_t totalSize = 0;
				// Combine all the blocks into one
				for (uint i = 0; i < numBlocks; ++i)
				{
					totalSize += m_Blocks[i]->m_Size;
					FreeBlock(m_Blocks[i]);
				}
				m_Blocks.Clear();

				AllocBlock(totalSize);
			}
			else
			{
				// Reset the position
				m_CurBlock->Clear();
			}
		}
	}

	size_t ScratchAllocator::GetTotalSize() const
	{
		size_t totalSize = 0;
		for (const Block* block : m_Blocks)
		{
			totalSize += block->m_Size;
		}
		return totalSize;
	}
}