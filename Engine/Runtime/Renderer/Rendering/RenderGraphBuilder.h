#pragma once

#include "RenderGraphTypes.h"

namespace tyr
{
    class RenderGraph;

    class RenderGraphBuilder
    {
    public:
        RenderGraphBuilder(RenderGraph& graph, uint passIndex);

        void ReadBuffer(const RenderBuffer& buffer, PipelineStage stage, BarrierAccess access);
        void WriteBuffer(const RenderBuffer& buffer, PipelineStage stage, BarrierAccess access);

        void ReadTexture(const Texture& texture, PipelineStage stage, BarrierAccess access, ImageLayout layout = IMAGE_LAYOUT_GENERAL);
        void WriteTexture(const Texture& texture, PipelineStage stage, BarrierAccess access, ImageLayout layout = IMAGE_LAYOUT_GENERAL);

    private:
        RenderGraph& m_Graph;
        uint m_PassIndex;
    };
	
}