#include "RenderGraphBuilder.h"
#include "RenderGraph.h"
#include "RenderAPI/Buffer.h"
#include "RenderResource/RenderBuffer.h"
#include "RenderResource/Texture.h"

namespace tyr
{
    RenderGraphBuilder::RenderGraphBuilder(RenderGraph& graph, uint passIndex)
        : m_Graph(graph)
        , m_PassIndex(passIndex)
    {
    }

    void RenderGraphBuilder::ReadBuffer(const RenderBuffer& buffer, PipelineStage stage, BarrierAccess access)
    {
        const uint bufferIndex = buffer.renderGraphIndex;
        RenderGraphBufferNode& node = m_Graph.m_BufferNodes[bufferIndex];

        if (node.usageSpan.count == 0)
            node.usageSpan.start = m_Graph.m_BufferUsages.Size();

        m_Graph.m_BufferUsages.Add({
            bufferIndex,
            m_PassIndex,
            stage,
            access,
            RenderGraphAccessType::Read,
            IMAGE_LAYOUT_UNKNOWN
            });

        node.usageSpan.count++;
    }

    void RenderGraphBuilder::WriteBuffer(const RenderBuffer& buffer, PipelineStage stage, BarrierAccess access)
    {
        const uint bufferIndex = buffer.renderGraphIndex;
        RenderGraphBufferNode& node = m_Graph.m_BufferNodes[bufferIndex];

        if (node.usageSpan.count == 0)
            node.usageSpan.start = m_Graph.m_BufferUsages.Size();

        m_Graph.m_BufferUsages.Add({
            bufferIndex,
            m_PassIndex,
            stage,
            access,
            RenderGraphAccessType::Write,
            IMAGE_LAYOUT_UNKNOWN
            });

        node.usageSpan.count++;
    }

    void RenderGraphBuilder::ReadTexture(const Texture& texture, PipelineStage stage, BarrierAccess access, ImageLayout layout)
    {
        const uint textureIndex = texture.renderGraphIndex;
        RenderGraphTextureNode& node = m_Graph.m_TextureNodes[textureIndex];

        if (node.usageSpan.count == 0)
            node.usageSpan.start = m_Graph.m_TextureUsages.Size();

        m_Graph.m_TextureUsages.Add({
            textureIndex,
            m_PassIndex,
            stage,
            access,
            RenderGraphAccessType::Read,
            layout
            });

        node.usageSpan.count++;
    }

    void RenderGraphBuilder::WriteTexture(const Texture& texture, PipelineStage stage, BarrierAccess access, ImageLayout layout)
    {
        const uint textureIndex = texture.renderGraphIndex;
        RenderGraphTextureNode& node = m_Graph.m_TextureNodes[textureIndex];

        if (node.usageSpan.count == 0)
            node.usageSpan.start = m_Graph.m_TextureUsages.Size();

        m_Graph.m_TextureUsages.Add({
            textureIndex,
            m_PassIndex,
            stage,
            access,
            RenderGraphAccessType::Write,
            layout
            });

        node.usageSpan.count++;
    }
}