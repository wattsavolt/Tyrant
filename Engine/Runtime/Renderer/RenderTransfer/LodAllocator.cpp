#include "LodAllocator.h"
#include "Core.h"
#include "RenderResource/MeshDesc.h"

namespace tyr
{
    LodAllocator::LodAllocator()
    {
        m_FreeChunks.Reserve(1024);
    }

    uint LodAllocator::Allocate()
    {
        if (!m_FreeChunks.IsEmpty())
        {
            const Chunk chunk = m_FreeChunks.Back();
            m_FreeChunks.PopBack();
            return chunk.offset;
        }

        const uint offset = m_ChunkOffset;
        m_ChunkOffset += MeshConstants::c_MaxLods;

        return offset;
    }

    void LodAllocator::Free(uint offset)
    {
        Chunk chunk;
        chunk.offset = offset;
        m_FreeChunks.Add(chunk);
    }

    void LodAllocator::Reset()
    {
        m_FreeChunks.Clear();
        m_ChunkOffset = 0;
    }
}