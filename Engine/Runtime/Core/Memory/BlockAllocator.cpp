#include "BlockAllocator.h"
#include "MemoryUtil.h"

namespace tyr
{
    BlockAllocator::Block::Block(size_t size)
        : m_Size(size)
    {
        m_Pos = 0;
        m_AllocCount = 0;
    }

    BlockAllocator::Block::~Block()
    {
        // m_Data freed inline with the block
    }

    void BlockAllocator::Block::SetDataPointer(uint8* data)
    {
        m_Data = data;
    }

    uint8* BlockAllocator::Block::Alloc(size_t size, size_t alignment)
    {
        // Reserve space for the back-pointer
        const size_t headerSize = sizeof(Block*);
        const uintptr_t current = reinterpret_cast<uintptr_t>(m_Data + m_Pos + headerSize);
        const uintptr_t aligned = MemoryUtil::Align<uintptr_t>(current, alignment);
        const size_t offset = static_cast<size_t>(aligned - reinterpret_cast<uintptr_t>(m_Data));

        if (offset + size > m_Size)
        {
            return nullptr;
        }

        m_Pos = offset + size;
        m_AllocCount++;

        // Store the back-pointer to this block immediately before user data
        Block** backPtr = reinterpret_cast<Block**>(m_Data + offset);
        *backPtr = this;

        return reinterpret_cast<uint8*>(backPtr + 1);
    }

    void BlockAllocator::Block::Free()
    {
        TYR_ASSERT(m_AllocCount > 0);
        m_AllocCount--;
    }

    bool BlockAllocator::Block::IsEmpty() const
    {
        return m_AllocCount == 0;
    }

    void BlockAllocator::Block::Reset()
    {
        m_Pos = 0;
        m_AllocCount = 0;
    }

    BlockAllocator::BlockAllocator(size_t minBlockSize)
        : m_MinBlockSize(minBlockSize)
        , m_CurBlock(nullptr)
    {
    }

    BlockAllocator::~BlockAllocator()
    {
        for (auto block : m_Blocks)
        {
            if (block)
            {
                FreeAligned16(reinterpret_cast<void*>(block));
            }
        }
    }

    uint8* BlockAllocator::Alloc(size_t size)
    {
        return AllocAligned(size, 16);
    }

    uint8* BlockAllocator::AllocAligned(size_t size, size_t alignment)
    {
        TYR_ASSERT((alignment & (alignment - 1)) == 0);

        // Try current block
        if (m_CurBlock)
        {
            uint8* ptr = m_CurBlock->Alloc(size, alignment);
            if (ptr)
                return ptr;
        }

        // Find empty block
        for (auto& block : m_Blocks)
        {
            if (block->IsEmpty() && block != m_CurBlock)
            {
                m_CurBlock = block;
                uint8* ptr = m_CurBlock->Alloc(size, alignment);
                if (ptr)
                    return ptr;
            }
        }

        // Allocate new block
        AllocBlock(size);
        TYR_ASSERT(m_CurBlock);
        return m_CurBlock->Alloc(size, alignment);
    }

    void BlockAllocator::Free(uint8* ptr)
    {
        if (!ptr) return;

        // The Block* is stored immediately before the user pointer
        Block** backPtr = reinterpret_cast<Block**>(ptr) - 1;
        Block* block = *backPtr;

        block->Free();
    }

    void BlockAllocator::Reset()
    {
        for (auto& block : m_Blocks)
        {
            block->Reset();
        }

        m_CurBlock = nullptr;
    }

    size_t BlockAllocator::GetTotalSize() const
    {
        size_t total = 0;
        for (auto& block : m_Blocks)
        {
            total += block->m_Size;
        }
        return total;
    }

    void BlockAllocator::AllocBlock(size_t size)
    {
        const uint blockSize = size > m_MinBlockSize ? size : m_MinBlockSize;

        // Total allocation: Block + data + alignment padding
        const size_t totalSize = sizeof(Block) + blockSize + 16;

        uint8* rawMem = reinterpret_cast<uint8*>(MemAllocAligned16(totalSize));

        // Compute alignment offset for user data pointer
        const uintptr_t rawAddr = reinterpret_cast<uintptr_t>(rawMem + sizeof(Block));
        const uintptr_t alignAddr = (rawAddr + 15) & ~uintptr_t(15);
        const size_t alignOffset = alignAddr - rawAddr;

        Block* block = new (rawMem) Block(blockSize);

        // Set data pointer after block header + alignment
        block->SetDataPointer(rawMem + sizeof(Block) + alignOffset);

        m_Blocks.Add(block);
        m_CurBlock = block;
    }
}