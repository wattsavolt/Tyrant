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

        uint AddPass(const char* name, RenderGraphPassSetupFn setup, RenderGraphPassExecuteFn&& execute, RenderGraphPhase phase = RenderGraphPhase::Output, bool enabled = true);

        void Compile();
        void Execute(CommandList& cmdList);

        void RegisterBuffer(RenderBuffer* buffer);
        void RegisterTexture(Texture* texture);

    private:
        friend class RenderGraphBuilder;

        void BuildBarriers();
        void BuildPassDependencies();
        void SortPassesDeterministically();

    private:
        RGArray<RenderGraphPassNode> m_PassNodes;
        RGArray<RenderGraphBufferNode> m_BufferNodes;
        RGArray<RenderGraphTextureNode> m_TextureNodes;

        RGArray<RenderGraphResourceUsage> m_BufferUsages;
        RGArray<RenderGraphResourceUsage> m_TextureUsages;

        RGArray<BufferRenderGraphBarrier> m_BufferBarriers;
        RGArray<TextureRenderGraphBarrier> m_TextureBarriers;

        RGArray<uint> m_PassDependencies;
    };
}