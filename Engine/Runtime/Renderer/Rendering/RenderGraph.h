#pragma once

#include "RenderGraphTypes.h"

namespace tyr
{
    class CommandList;
    class RenderGraphBuilder;

    // Function type for the setup callback in a render graph pass
    using RenderGraphPassSetupFn = FunctionRef<void(RenderGraphBuilder&)>;

    class RenderGraph
    {
    public:
        RenderGraph() = default;

        uint AddPass(const char* name, RenderGraphPassSetupFn setup, RenderGraphPassExecuteFn&& execute, RenderGraphPhase phase = RenderGraphPhase::Output, CommandQueueType queueType = CommandQueueType::CQ_GRAPHICS, bool enabled = true);

        void Compile();

        // cmdLists must have one entry per CommandQueueType - only the ones actually used by
        // a pass need to be non-null.
        void Execute(CommandList* const cmdLists[CommandQueueType::CQ_COUNT]);

        void RegisterBuffer(RenderBuffer* buffer);
        void RegisterTexture(Texture* texture);
        void RegisterAccelerationStructure(RenderAccelerationStructure* accelerationStructure);

    private:
        friend class RenderGraphBuilder;

        void BuildBarriers();

    private:
        RGArray<RenderGraphPassNode> m_PassNodes;
        RGArray<RenderGraphBufferNode> m_BufferNodes;
        RGArray<RenderGraphTextureNode> m_TextureNodes;
        RGArray<RenderGraphAccelerationStructureNode> m_AccelerationStructureNodes;

        // Pass indices bucketed by phase, in registration order - gives Execute() deterministic
        // phase ordering without ever physically reordering m_PassNodes, which must stay
        // append-only and index-stable for resource usages to keep indexing into it correctly.
        RGArray<uint> m_PassOrder[(uint)RenderGraphPhase::Count];
    };
}
