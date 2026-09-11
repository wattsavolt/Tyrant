#include "RenderGraph.h"
#include "RenderGraphBuilder.h"
#include "RenderAPI/Buffer.h"
#include "RenderAPI/CommandList.h"
#include "RenderResource/RenderBuffer.h"
#include "RenderResource/Texture.h"

namespace tyr
{
    // ----------------------------------------------------------------
    // Resource registration
    // ----------------------------------------------------------------
    void RenderGraph::RegisterBuffer(RenderBuffer* buffer)
    {
        buffer->renderGraphIndex = m_BufferNodes.Size();
        m_BufferNodes.Add({ buffer });
    }

    void RenderGraph::RegisterTexture(Texture* texture)
    {
        texture->renderGraphIndex = m_TextureNodes.Size();
        m_TextureNodes.Add({ texture });
    }

    // ----------------------------------------------------------------
    // Add pass
    // ----------------------------------------------------------------
    uint RenderGraph::AddPass(const char* name, RenderGraphPassSetupFn setup, RenderGraphPassExecuteFn&& execute, RenderGraphPhase phase, bool enabled)
    {
        if (!enabled) 
            return c_RenderGraphInvalidIndex;

        const uint index = m_PassNodes.Size();
        m_PassNodes.Add({ name, std::move(execute), phase });

        RenderGraphBuilder builder(*this, index);
        setup(builder);

        return index;
    }

    // ----------------------------------------------------------------
    // Build dependencies between passes via resources
    // ----------------------------------------------------------------
    void RenderGraph::BuildPassDependencies()
    {
        for (uint i = 0; i < m_BufferNodes.Size(); ++i)
        {
            const RenderGraphBufferNode& node = m_BufferNodes[i];
            const RenderGraphSpan span = node.usageSpan;

            for (uint u = 1; u < span.count; ++u)
            {
                const RenderGraphResourceUsage& prev = m_BufferUsages[span.start + u - 1];
                const RenderGraphResourceUsage& curr = m_BufferUsages[span.start + u];

                if (prev.passIndex == curr.passIndex)
                    continue;

                RenderGraphPassNode& pass = m_PassNodes[curr.passIndex];

                if (pass.dependencySpan.count == 0)
                    pass.dependencySpan.start = m_PassDependencies.Size();

                m_PassDependencies.Add(prev.passIndex);
                pass.dependencySpan.count++;
            }
        }

        for (uint i = 0; i < m_TextureNodes.Size(); ++i)
        {
            const RenderGraphTextureNode& node = m_TextureNodes[i];
            const RenderGraphSpan span = node.usageSpan;

            for (uint u = 1; u < span.count; ++u)
            {
                const RenderGraphResourceUsage& prev = m_TextureUsages[span.start + u - 1];
                const RenderGraphResourceUsage& curr = m_TextureUsages[span.start + u];

                if (prev.passIndex == curr.passIndex)
                    continue;

                RenderGraphPassNode& pass = m_PassNodes[curr.passIndex];

                if (pass.dependencySpan.count == 0)
                    pass.dependencySpan.start = m_PassDependencies.Size();

                m_PassDependencies.Add(prev.passIndex);
                pass.dependencySpan.count++;
            }
        }
    }

    // ----------------------------------------------------------------
    // Build render barriers
    // ----------------------------------------------------------------
    void RenderGraph::BuildBarriers()
    {
        // ------------------------------------------------------------
        // Buffers
        // ------------------------------------------------------------
        for (uint i = 0; i < m_BufferNodes.Size(); ++i)
        {
            RenderGraphBufferNode& node = m_BufferNodes[i];
            const RenderGraphSpan span = node.usageSpan;

            BarrierAccess currentAccess = node.buffer->accessState;

            for (uint u = 0; u < span.count; ++u)
            {
                const RenderGraphResourceUsage& usage =
                    m_BufferUsages[span.start + u];

                if (currentAccess == usage.access)
                    continue;

                BufferBarrier barrier{};
                barrier.buffer = node.buffer->buffer;
                barrier.srcStage = PIPELINE_STAGE_ALL_COMMANDS_BIT;
                barrier.srcAccess = currentAccess;
                barrier.dstStage = usage.stage;
                barrier.dstAccess = usage.access;

                RenderGraphPassNode& pass = m_PassNodes[usage.passIndex];

                if (pass.bufferBarrierSpan.count == 0)
                    pass.bufferBarrierSpan.start = m_BufferBarriers.Size();

                m_BufferBarriers.Add({
                    usage.passIndex,
                    i,
                    barrier
                    });

                pass.bufferBarrierSpan.count++;
                currentAccess = usage.access;
            }

            // Persist final state for next frame
            node.buffer->accessState = currentAccess;
        }

        // ------------------------------------------------------------
        // Textures
        // ------------------------------------------------------------
        for (uint i = 0; i < m_TextureNodes.Size(); ++i)
        {
            RenderGraphTextureNode& node = m_TextureNodes[i];
            const RenderGraphSpan span = node.usageSpan;

            BarrierAccess currentAccess = node.texture->accessState;
            ImageLayout currentLayout = node.texture->imageLayout;

            for (uint u = 0; u < span.count; ++u)
            {
                const RenderGraphResourceUsage& usage =
                    m_TextureUsages[span.start + u];

                if (currentAccess == usage.access && currentLayout == usage.layout)
                    continue;

                ImageBarrier barrier{};
                barrier.image = node.texture->image;
                barrier.srcStage = PIPELINE_STAGE_ALL_COMMANDS_BIT;
                barrier.srcAccess = currentAccess;
                barrier.srcLayout = currentLayout;
                barrier.dstStage = usage.stage;
                barrier.dstAccess = usage.access;
                barrier.dstLayout = usage.layout;

                RenderGraphPassNode& pass = m_PassNodes[usage.passIndex];

                if (pass.textureBarrierSpan.count == 0)
                    pass.textureBarrierSpan.start = m_TextureBarriers.Size();

                m_TextureBarriers.Add({
                    usage.passIndex,
                    i,
                    barrier
                    });

                pass.textureBarrierSpan.count++;
                currentAccess = usage.access;
                currentLayout = usage.layout;
            }

            // Persist final state for next frame
            node.texture->accessState = currentAccess;
            node.texture->imageLayout = currentLayout;
        }
    }


    // ----------------------------------------------------------------
    // Sort passes deterministically by phase + registration order
    // ----------------------------------------------------------------
    void RenderGraph::SortPassesDeterministically()
    {
        std::stable_sort(m_PassNodes.begin(), m_PassNodes.end(),
            [](const RenderGraphPassNode& a, const RenderGraphPassNode& b) {
                return a.phase < b.phase;
            });
    }

    // ----------------------------------------------------------------
    // Compile
    // ----------------------------------------------------------------
    void RenderGraph::Compile()
    {
        BuildPassDependencies();
        BuildBarriers();
        SortPassesDeterministically();
    }

    // ----------------------------------------------------------------
    // Execute
    // ----------------------------------------------------------------
    void RenderGraph::Execute(CommandList& cmdList)
    {
        for (uint i = 0; i < m_PassNodes.Size(); ++i)
        {
            const RenderGraphPassNode& pass = m_PassNodes[i];

            if (pass.bufferBarrierSpan.count != 0 || pass.textureBarrierSpan.count != 0)
            {
                cmdList.AddBarriers(
                    pass.bufferBarrierSpan.count != 0
                    ? &m_BufferBarriers[pass.bufferBarrierSpan.start].barrier
                    : nullptr,
                    pass.bufferBarrierSpan.count,
                    pass.textureBarrierSpan.count != 0
                    ? &m_TextureBarriers[pass.textureBarrierSpan.start].barrier
                    : nullptr,
                    pass.textureBarrierSpan.count
                );
            }

            pass.execute.Invoke(cmdList);
        }
    }
}