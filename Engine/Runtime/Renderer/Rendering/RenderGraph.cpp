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
    uint RenderGraph::AddPass(const char* name, RenderGraphPassSetupFn setup, RenderGraphPassExecuteFn&& execute, RenderGraphPhase phase, CommandQueueType queueType, bool enabled)
    {
        if (!enabled)
            return c_RenderGraphInvalidIndex;

        const uint index = m_PassNodes.Size();
        m_PassNodes.Add({ name, std::move(execute), phase, queueType });
        // Deterministic execution order without ever moving RenderGraphPassNode objects around.
        m_PassOrder[(uint)phase].Add(index);

        RenderGraphBuilder builder(*this, index);
        setup(builder);

        return index;
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

            BarrierAccess currentAccess = node.buffer->accessState;
            CommandQueueType currentQueueType = node.buffer->queueTypeState;
            RenderGraphAccessType currentAccessType = node.buffer->lastAccessType;
            PipelineStage currentStageMask = node.buffer->lastStageMask;

            for (const RenderGraphResourceUsage& usage : node.usages)
            {
                RenderGraphPassNode& pass = m_PassNodes[usage.passIndex];

                // A same-queue pipeline barrier can't synchronize against a different
                // queue's prior access - that's handled by a cross-queue semaphore wait
                // instead (the buffer is concurrent-shared, so no ownership transfer is
                // needed either).
                if (currentQueueType != pass.queueType)
                {
                    currentAccess = usage.access;
                    currentAccessType = usage.accessType;
                    currentStageMask = usage.stage;
                    currentQueueType = pass.queueType;
                    continue;
                }

                // Read-after-read only skips a barrier if the new read's stage was already
                // covered by an earlier one - a read at a different, not-yet-covered stage still
                // needs its own barrier even though it's still just a read.
                const bool stageAlreadyCovered = (currentStageMask & usage.stage) == usage.stage;
                const bool bothReads = currentAccessType == RenderGraphAccessType::Read
                    && usage.accessType == RenderGraphAccessType::Read;
                if (bothReads && stageAlreadyCovered)
                    continue;

                BufferBarrier barrier{};
                barrier.buffer = node.buffer->buffer;
                barrier.srcStage = PIPELINE_STAGE_ALL_COMMANDS_BIT;
                barrier.srcAccess = currentAccess;
                barrier.dstStage = usage.stage;
                barrier.dstAccess = usage.access;

                // Straight into the owning pass' own array - already exactly what Execute()
                // needs, in decision order, with no further grouping step.
                pass.bufferBarriers.Add(barrier);

                // A write resets coverage to just its own stage; a read-after-read instead
                // extends coverage, since every stage already synchronized stays valid.
                currentStageMask = bothReads
                    ? static_cast<PipelineStage>(currentStageMask | usage.stage)
                    : usage.stage;
                currentAccess = usage.access;
                currentAccessType = usage.accessType;
            }

            // Persist final state for next frame
            node.buffer->accessState = currentAccess;
            node.buffer->queueTypeState = currentQueueType;
            node.buffer->lastAccessType = currentAccessType;
            node.buffer->lastStageMask = currentStageMask;
        }

        // ------------------------------------------------------------
        // Textures
        // ------------------------------------------------------------
        for (uint i = 0; i < m_TextureNodes.Size(); ++i)
        {
            RenderGraphTextureNode& node = m_TextureNodes[i];

            BarrierAccess currentAccess = node.texture->accessState;
            ImageLayout currentLayout = node.texture->imageLayout;
            CommandQueueType currentQueueType = node.texture->queueTypeState;
            RenderGraphAccessType currentAccessType = node.texture->lastAccessType;
            PipelineStage currentStageMask = node.texture->lastStageMask;

            for (const RenderGraphResourceUsage& usage : node.usages)
            {
                RenderGraphPassNode& pass = m_PassNodes[usage.passIndex];

                // A different queue's prior access is synchronized via a semaphore wait, not
                // a same-queue barrier.
                if (currentQueueType != pass.queueType)
                {
                    currentAccess = usage.access;
                    currentLayout = usage.layout;
                    currentAccessType = usage.accessType;
                    currentStageMask = usage.stage;
                    currentQueueType = pass.queueType;
                    continue;
                }

                // A layout change always needs a transition, even read-to-read, regardless of
                // whether the stage itself was already covered.
                const bool stageAlreadyCovered = (currentStageMask & usage.stage) == usage.stage;
                const bool bothReads = currentAccessType == RenderGraphAccessType::Read
                    && usage.accessType == RenderGraphAccessType::Read;
                if (bothReads && stageAlreadyCovered && currentLayout == usage.layout)
                    continue;

                ImageBarrier barrier{};
                barrier.image = node.texture->image;
                barrier.srcStage = PIPELINE_STAGE_ALL_COMMANDS_BIT;
                barrier.srcAccess = currentAccess;
                barrier.srcLayout = currentLayout;
                barrier.dstStage = usage.stage;
                barrier.dstAccess = usage.access;
                barrier.dstLayout = usage.layout;

                pass.textureBarriers.Add(barrier);

                // Extends coverage on a read-after-read; resets to just this usage's stage
                // otherwise, since a write or layout change invalidates prior visibility.
                currentStageMask = (bothReads && currentLayout == usage.layout)
                    ? static_cast<PipelineStage>(currentStageMask | usage.stage)
                    : usage.stage;
                currentAccess = usage.access;
                currentLayout = usage.layout;
                currentAccessType = usage.accessType;
            }

            // Persist final state for next frame
            node.texture->accessState = currentAccess;
            node.texture->imageLayout = currentLayout;
            node.texture->queueTypeState = currentQueueType;
            node.texture->lastAccessType = currentAccessType;
            node.texture->lastStageMask = currentStageMask;
        }
    }

    // ----------------------------------------------------------------
    // Compile
    // ----------------------------------------------------------------
    void RenderGraph::Compile()
    {
        BuildBarriers();
    }

    // ----------------------------------------------------------------
    // Execute
    // ----------------------------------------------------------------
    void RenderGraph::Execute(CommandList* const cmdLists[CommandQueueType::CQ_COUNT])
    {
        for (uint phase = 0; phase < (uint)RenderGraphPhase::Count; ++phase)
        {
            for (uint passIndex : m_PassOrder[phase])
            {
                RenderGraphPassNode& pass = m_PassNodes[passIndex];

                CommandList& cmdList = *cmdLists[pass.queueType];

                if (!pass.bufferBarriers.IsEmpty() || !pass.textureBarriers.IsEmpty())
                {
                    cmdList.AddBarriers(
                        pass.bufferBarriers.IsEmpty() ? nullptr : pass.bufferBarriers.Data(),
                        pass.bufferBarriers.Size(),
                        pass.textureBarriers.IsEmpty() ? nullptr : pass.textureBarriers.Data(),
                        pass.textureBarriers.Size()
                    );
                }

                pass.execute.Invoke(cmdList);
            }
        }
    }
}
