#include "RenderGraph.h"
#include "RenderGraphBuilder.h"
#include "RenderAPI/Buffer.h"
#include "RenderAPI/CommandList.h"
#include "RenderResource/RenderBuffer.h"
#include "RenderResource/Texture.h"
#include "Memory/StackAllocation.h"

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
    uint RenderGraph::AddPass(const char* name, RenderGraphPassSetupFn setup, RenderGraphPassExecuteFn&& execute, RenderGraphPhase phase, CommandQueueType queueType, bool enabled)
    {
        if (!enabled)
            return c_RenderGraphInvalidIndex;

        const uint index = m_PassNodes.Size();
        m_PassNodes.Add({ name, std::move(execute), phase, queueType });

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
            CommandQueueType currentQueueType = node.buffer->queueTypeState;

            for (uint u = 0; u < span.count; ++u)
            {
                const RenderGraphResourceUsage& usage =
                    m_BufferUsages[span.start + u];

                RenderGraphPassNode& pass = m_PassNodes[usage.passIndex];

                // A same-queue pipeline barrier can't synchronize against a different
                // queue's prior access - that's handled by a cross-queue semaphore wait
                // instead (the buffer is concurrent-shared, so no ownership transfer is
                // needed either).
                if (currentQueueType != pass.queueType)
                {
                    currentAccess = usage.access;
                    currentQueueType = pass.queueType;
                    continue;
                }

                if (currentAccess == usage.access)
                    continue;

                BufferBarrier barrier{};
                barrier.buffer = node.buffer->buffer;
                barrier.srcStage = PIPELINE_STAGE_ALL_COMMANDS_BIT;
                barrier.srcAccess = currentAccess;
                barrier.dstStage = usage.stage;
                barrier.dstAccess = usage.access;

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
            node.buffer->queueTypeState = currentQueueType;
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
            CommandQueueType currentQueueType = node.texture->queueTypeState;

            for (uint u = 0; u < span.count; ++u)
            {
                const RenderGraphResourceUsage& usage =
                    m_TextureUsages[span.start + u];

                RenderGraphPassNode& pass = m_PassNodes[usage.passIndex];

                // Same reasoning as the buffer loop above - a different queue's prior
                // access is synchronized via a semaphore wait, not a same-queue barrier.
                if (currentQueueType != pass.queueType)
                {
                    currentAccess = usage.access;
                    currentLayout = usage.layout;
                    currentQueueType = pass.queueType;
                    continue;
                }

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
            node.texture->queueTypeState = currentQueueType;
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
    void RenderGraph::Execute(CommandList* const cmdLists[CommandQueueType::CQ_COUNT])
    {
        for (uint i = 0; i < m_PassNodes.Size(); ++i)
        {
            const RenderGraphPassNode& pass = m_PassNodes[i];

            CommandList& cmdList = *cmdLists[pass.queueType];

            if (pass.bufferBarrierSpan.count != 0 || pass.textureBarrierSpan.count != 0)
            {
                // m_BufferBarriers/m_TextureBarriers store a BufferBarrier/ImageBarrier embedded
                // inside a larger struct (alongside passIndex/bufferIndex-or-textureIndex
                // bookkeeping) - a span of them is NOT contiguous BufferBarrier/ImageBarrier
                // data, so &m_BufferBarriers[start].barrier can't just be indexed as one. Copy
                // just the barrier out of each entry into its own tightly-packed array instead -
                // this data is only needed for the AddBarriers call below, so a stack allocation
                // is enough; no need for it to live in the render graph's own frame allocator.
                const SmartStack<BufferBarrier> bufferBarrierStack = SmartStackAlloc<BufferBarrier>(pass.bufferBarrierSpan.count);
                BufferBarrier* const bufferBarriers = bufferBarrierStack;
                for (uint b = 0; b < pass.bufferBarrierSpan.count; ++b)
                {
                    bufferBarriers[b] = m_BufferBarriers[pass.bufferBarrierSpan.start + b].barrier;
                }

                const SmartStack<ImageBarrier> textureBarrierStack = SmartStackAlloc<ImageBarrier>(pass.textureBarrierSpan.count);
                ImageBarrier* const textureBarriers = textureBarrierStack;
                for (uint t = 0; t < pass.textureBarrierSpan.count; ++t)
                {
                    textureBarriers[t] = m_TextureBarriers[pass.textureBarrierSpan.start + t].barrier;
                }

                cmdList.AddBarriers(
                    pass.bufferBarrierSpan.count != 0 ? bufferBarriers : nullptr,
                    pass.bufferBarrierSpan.count,
                    pass.textureBarrierSpan.count != 0 ? textureBarriers : nullptr,
                    pass.textureBarrierSpan.count
                );
            }

            pass.execute.Invoke(cmdList);
        }
    }
}