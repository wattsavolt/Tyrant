#pragma once

#include "RendererMacros.h"
#include "Core.h"

namespace tyr
{
    class LodAllocator final
    {
    public:
        LodAllocator();

        // Allocates one chunk and returns the offset into the LOD buffer
        uint Allocate();

        // Frees a previously allocated chunk (using offset)
        void Free(uint offset);

        void Reset();

    private:
        struct Chunk
        {
            uint offset; // offset into LOD buffer
        };

        Array<Chunk> m_FreeChunks;

        // Offset in numbers of LODs 
        uint m_ChunkOffset = 0;
    };
}
