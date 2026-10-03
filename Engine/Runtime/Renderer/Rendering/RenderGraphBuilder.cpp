#include "RenderGraphBuilder.h"
#include "RenderGraph.h"
#include "RenderAPI/Buffer.h"
#include "RenderResource/RenderBuffer.h"
#include "RenderResource/Texture.h"
#include "RenderResource/RenderAccelerationStructure.h"

namespace tyr
{
    RenderGraphBuilder::RenderGraphBuilder(RenderGraph& graph, uint passIndex)
        : m_Graph(graph)
        , m_PassIndex(passIndex)
    {
    }

    // Appended straight into the owning RenderGraphBufferNode's own usages array (via
    // buffer.renderGraphIndex) - already exactly grouped by resource, in the order Setup() calls
    // record them, with no separate grouping/sort step needed afterward.
    void RenderGraphBuilder::ReadBuffer(const RenderBuffer& buffer, PipelineStage stage, BarrierAccess access)
    {
        m_Graph.m_BufferNodes[buffer.renderGraphIndex].usages.Add({
            m_PassIndex,
            stage,
            access,
            RenderGraphAccessType::Read,
            IMAGE_LAYOUT_UNKNOWN
            });
    }

    void RenderGraphBuilder::WriteBuffer(const RenderBuffer& buffer, PipelineStage stage, BarrierAccess access)
    {
        m_Graph.m_BufferNodes[buffer.renderGraphIndex].usages.Add({
            m_PassIndex,
            stage,
            access,
            RenderGraphAccessType::Write,
            IMAGE_LAYOUT_UNKNOWN
            });
    }

    void RenderGraphBuilder::ReadTexture(const Texture& texture, PipelineStage stage, BarrierAccess access, ImageLayout layout)
    {
        m_Graph.m_TextureNodes[texture.renderGraphIndex].usages.Add({
            m_PassIndex,
            stage,
            access,
            RenderGraphAccessType::Read,
            layout
            });
    }

    void RenderGraphBuilder::WriteTexture(const Texture& texture, PipelineStage stage, BarrierAccess access, ImageLayout layout)
    {
        m_Graph.m_TextureNodes[texture.renderGraphIndex].usages.Add({
            m_PassIndex,
            stage,
            access,
            RenderGraphAccessType::Write,
            layout
            });
    }

    void RenderGraphBuilder::ReadAccelerationStructure(const RenderAccelerationStructure& accelerationStructure, PipelineStage stage, BarrierAccess access)
    {
        m_Graph.m_AccelerationStructureNodes[accelerationStructure.renderGraphIndex].usages.Add({
            m_PassIndex,
            stage,
            access,
            RenderGraphAccessType::Read,
            IMAGE_LAYOUT_UNKNOWN
            });
    }

    void RenderGraphBuilder::WriteAccelerationStructure(const RenderAccelerationStructure& accelerationStructure, PipelineStage stage, BarrierAccess access)
    {
        m_Graph.m_AccelerationStructureNodes[accelerationStructure.renderGraphIndex].usages.Add({
            m_PassIndex,
            stage,
            access,
            RenderGraphAccessType::Write,
            IMAGE_LAYOUT_UNKNOWN
            });
    }
}
