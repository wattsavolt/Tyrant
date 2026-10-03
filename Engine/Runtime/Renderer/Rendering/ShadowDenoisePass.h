#pragma once

#include "Core.h"
#include "RendererMacros.h"
#include "RenderAPI/Pipeline.h"
#include "RenderBase/RenderHandles.h"
#include "RenderConstants.h"

namespace tyr
{
	class CommandList;
	class RenderRegistry;
	class RenderGraphBuilder;
	struct RenderResources;

	struct ShadowDenoisePassArgs
	{
		RenderRegistry* registry;
		RenderResources* resources;
		ComputePipelineHandle pipeline;
	};

	// Matches PushConstants in ShadowDenoiseCS.hlsl byte for byte.
	struct ShadowDenoisePushConstants
	{
		uint depthIndex;
		uint motionIndex;
		uint width;
		uint height;
		uint renderFrameIndex;
		uint prevRenderFrameIndex;
		uint slot;
		// 0/1 - false on this viewport slot's first-ever tick, or whenever the previous slot's
		// own shadow masks don't match this tick's resolution (a resize is still propagating
		// across buffered slots) - both cases mean there's nothing valid to blend with yet.
		uint hasHistory;
		uint spatialRadius;
	};

	// Runs once per active shadow slot this tick (the same slots ShadowRTPass just traced):
	// a small depth-aware spatial blur of this tick's raw trace, then a temporal blend against
	// the previous buffered viewport slot's own denoised result (reprojected via gbufferMotion),
	// writing the final result into this tick's own slot - which next tick's denoise pass will
	// in turn read as its own history, the same cross-slot-reuse pattern TAA's colour history
	// uses.
	class ShadowDenoisePass final : public INonCopyable
	{
	public:
		ShadowDenoisePass(const ShadowDenoisePassArgs& args);
		~ShadowDenoisePass();

		void Recreate(const ShadowDenoisePassArgs& args);

		// prevShadowMasks is the *previous* buffered viewport slot's own shadowMasks texture
		// (a distinct resource from this tick's shadowMasks) - only read when hasHistory is true.
		void Setup(RenderGraphBuilder& builder, TextureHandle depthBuffer, TextureHandle gbufferMotion,
			TextureHandle shadowMasksRaw, TextureHandle shadowMasks, TextureHandle prevShadowMasks, bool hasHistory);

		void Execute(CommandList& cmdList, const LocalArray<uint, RenderConstants::c_MaxShadowSlots>& activeSlots,
			uint renderFrameIndex, uint prevRenderFrameIndex, bool hasHistory, uint width, uint height, uint spatialRadius);

	private:
		RenderRegistry* m_Registry;
		RenderResources* m_Resources;
		ComputePipelineHandle m_Pipeline;

		TextureHandle m_DepthBuffer;
		TextureHandle m_GBufferMotion;
	};
}
