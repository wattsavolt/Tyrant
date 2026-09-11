#pragma once

#include "Core.h"
#include "EngineMacros.h"

namespace tyr
{
    // Just a raw block of memory handed out by EcsChunkPool. Nothing reads or writes
    // this directly - it's always accessed through an EcsColumn, which knows how to
    // find the right spot inside it.
    struct EcsChunk
    {
        static constexpr size_t c_ChunkSizeBytes = 16 * 1024;
        uint8 data[c_ChunkSizeBytes];
    };

    constexpr uint c_MaxChunks = 1024;

    // One pool of fixed-size chunks, shared by the whole engine. Every EcsColumn -
    // whether it's holding entity IDs or component data, for any archetype - gets its
    // storage from here, so a chunk one archetype stops needing is ready for another
    // to pick up right away.
    class TYR_ENGINE_API EcsChunkPool final
    {
    public:
        static EcsChunkPool& Instance();

        Handle Create()
        {
            return m_Pool.Create();
        }

        void Delete(Handle h)
        {
            m_Pool.Delete(h);
        }

        EcsChunk& operator[](Handle h)
        {
            return m_Pool[h];
        }

        const EcsChunk& operator[](Handle h) const
        {
            return m_Pool[h];
        }

    private:
        EcsChunkPool() : m_Pool(c_MaxChunks) {}
        ~EcsChunkPool() = default;

        ObjectPool<EcsChunk> m_Pool;
    };
}
