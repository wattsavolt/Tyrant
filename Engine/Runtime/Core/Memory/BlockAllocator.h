#pragma once

#include "Allocation.h"
#include "Utility/Utility.h"
#include "Containers/Array.h"

namespace tyr
{
    /// Allocates fixed-size blocks and allows allocations of arbitrary size.
    /// Allocations and freeing should only happen on the same thread, and blocks are reused only when empty.
    class TYR_CORE_API BlockAllocator final
    {
    private:
        /// Single memory block
        class Block final
        {
        public:
            Block(size_t size);
            ~Block();

            /// Allocate memory from this block with given alignment
            uint8* Alloc(size_t size, size_t alignment);

            /// Notify that an allocation is freed
            void Free();

            /// Check if block is completely free
            bool IsEmpty() const;

            /// Reset block to reuse
            void Reset();

            /// Initialize the user memory pointer after placement
            void SetDataPointer(uint8* data);

        public:
            uint8* m_Data = nullptr;
            size_t m_Pos = 0;
            size_t m_Size = 0;
            size_t m_AllocCount = 0;
        };

    public:
        BlockAllocator(size_t minBlockSize = 1024 * 1024);
        ~BlockAllocator();

        /// Allocate memory of given size with default 16-byte alignment
        uint8* Alloc(size_t size);

        /// Allocate memory with specific alignment
        uint8* AllocAligned(size_t size, size_t alignment);

        /// Free an allocation (main thread only)
        void Free(uint8* ptr);

        /// Reset allocator, reusing all blocks
        void Reset();

        /// Total memory allocated
        size_t GetTotalSize() const;

    private:
        /// Allocate new block
        void AllocBlock(size_t size);

    private:
        size_t m_MinBlockSize;
        Array<Block*> m_Blocks;
        Block* m_CurBlock = nullptr;
    };
}



