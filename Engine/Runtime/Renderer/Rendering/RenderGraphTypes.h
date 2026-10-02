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

    // Resource usage record - no identity field, since each one is appended directly into its
    // owning resource node's own usages array, so the node it sits in already identifies it.
    struct RenderGraphResourceUsage
    {
        uint passIndex;
        PipelineStage stage;
        BarrierAccess access;
        RenderGraphAccessType accessType;
        ImageLayout layout; // images only
    };

    // Resource nodes - usages is appended to directly at setup time, already in per-resource
    // order with no separate grouping/sort step needed.
    struct RenderGraphBufferNode
    {
        RenderBuffer* buffer;
        RGArray<RenderGraphResourceUsage> usages;
    };

    struct RenderGraphTextureNode
    {
        Texture* texture;
        RGArray<RenderGraphResourceUsage> usages;
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
        // Which queue's command list this pass records into and submits on.
        CommandQueueType queueType;
        // Already exactly this pass' own barriers, in decision order, ready to issue with no
        // further copying or grouping.
        RGArray<BufferBarrier> bufferBarriers;
        RGArray<ImageBarrier> textureBarriers;
    };

}
