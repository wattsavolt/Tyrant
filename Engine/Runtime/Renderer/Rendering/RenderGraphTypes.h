#pragma once

#include "Core.h"
#include "RendererMacros.h"
#include "RenderAPI/RenderAPI.h"
#include "RenderAPI/Sync.h"
#include "RenderGraphAllocation.h"

namespace tyr
{
    class CommandList;
    struct RenderBuffer;
    struct Texture;

    using RenderGraphPassExecuteFn = Function<void(CommandList&)>;

    static constexpr uint c_RenderGraphInvalidIndex = ~0u;

    struct RenderGraphSpan
    {
        uint start = 0;
        uint count = 0;
    };

    // Resource usage types
    enum class RenderGraphResourceType : uint8
    {
        Buffer,
        Image
    };

    enum class RenderGraphAccessType : uint8
    {
        Read,
        Write
    };

    // Resource usage record
    struct RenderGraphResourceUsage
    {
        uint resourceIndex;
        uint passIndex;
        PipelineStage stage;
        BarrierAccess access;
        RenderGraphAccessType accessType;
        ImageLayout layout; // images only
    };

    // Resource nodes
    struct RenderGraphBufferNode
    {
        RenderBuffer* buffer;
        RenderGraphSpan usageSpan;
    };

    struct RenderGraphTextureNode
    {
        Texture* texture;
        RenderGraphSpan usageSpan;
    };

    struct BufferRenderGraphBarrier
    {
        uint passIndex;
        uint bufferIndex;
        BufferBarrier barrier;
    };

    struct TextureRenderGraphBarrier
    {
        uint passIndex;
        uint textureIndex;
        ImageBarrier barrier;
    };

    // Pipeline phases for deterministic ordering
    enum class RenderGraphPhase : uint8
    {
        Transfer = 0,
        Geometry,
        RayTracing,
        Post,
        Output,
        Count
    };

    struct RenderGraphPassNode
    {
        const char* name;
        RenderGraphPassExecuteFn execute;
        // Phase for deterministic ordering
        RenderGraphPhase phase;
        RenderGraphSpan bufferBarrierSpan;
        RenderGraphSpan textureBarrierSpan;
        RenderGraphSpan dependencySpan;
    };

}