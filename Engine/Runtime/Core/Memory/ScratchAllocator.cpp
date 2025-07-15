#include "Memory/ScratchAllocator.h"

namespace tyr
{
	ScratchAllocator::ScratchAllocator(uint blockSize)
		: m_MinBlockSize(blockSize)
		, m_CurBlock(nullptr)
	{
		TYR_ASSERT(blockSize > 0);
	}

	ScratchAllocator::~ScratchAllocator()
	{
		const uint numBlocks = static_cast<uint>(m_Blocks.Size());
		for (uint i = 0; i < numBlocks; ++i)
		{
			FreeBlock(m_Blocks[i]);
		}
	}

	uint8* ScratchAllocator::Alloc(uint amount)
	{
		TYR_ASSERT(amount != 0);

		uint freeMem = 0;
		if (m_CurBlock)
		{
			freeMem = m_CurBlock->m_Size - m_CurBlock->m_Pos;
		}

		if (amount > freeMem)
		{
			AllocBlock(amount);
		}

		return m_CurBlock->Alloc(amount);
	}

	uint8* ScratchAllocator::AllocAligned(uint amount, uint alignment)
	{
		uint freeMem = 0;
		uint pos = 0;

		if (m_CurBlock)
		{
			freeMem = m_CurBlock->m_Size - m_CurBlock->m_Pos;
			pos = m_CurBlock->m_Pos;
		}

		uint alignOffset = (alignment - (pos & (alignment - 1))) & (alignment - 1);
		if (amount + alignOffset > freeMem)
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

			AllocBlock(amount + alignOffset);
		}

		amount += alignOffset;

		uint8* data = m_CurBlock->Alloc(amount);
		// The data will begin after the offset as it may be padded to have 16 byte alignment
		return data + alignOffset;
	}

	void ScratchAllocator::AllocBlock(uint amount)
	{
		uint blockSize = m_MinBlockSize;
		if (amount > blockSize)
		{
			blockSize = amount;
		}

		const uint alignOffset = 16 - (sizeof(Block) & (16 - 1));

		uint8* data = reinterpret_cast<uint8*>(AllocAligned16(blockSize + sizeof(Block) + alignOffset));
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
			const uint numBlocks = static_cast<uint>(m_Blocks.Size());
			if (numBlocks > 1)
			{
				uint totalSize = 0;
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

	uint ScratchAllocator::GetTotalSize() const
	{
		uint totalSize = 0;
		for (const Block* block : m_Blocks)
		{
			totalSize += block->m_Size;
		}
		return totalSize;
	}
}