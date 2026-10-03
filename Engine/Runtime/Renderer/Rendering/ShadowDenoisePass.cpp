#include "ShadowDenoisePass.h"
#include "RenderAPI/CommandList.h"
#include "Rendering/RenderRegistry.h"
#include "Rendering/RenderGraphBuilder.h"
#include "Rendering/RenderResources.h"
#include "RenderResource/Texture.h"

namespace tyr
{
	ShadowDenoisePass::ShadowDenoisePass(const ShadowDenoisePassArgs& args)
	{
		Recreate(args);
	}

	ShadowDenoisePass::~ShadowDenoisePass()
	{
	}

	void ShadowDenoisePass::Recreate(const ShadowDenoisePassArgs& args)
	{
		m_Registry = args.registry;
		m_Resources = args.resources;
		m_Pipeline = args.pipeline;
	}

	void ShadowDenoisePass::Setup(RenderGraphBuilder& builder, TextureHandle depthBuffer, TextureHandle gbufferMotion,
		TextureHandle shadowMasksRaw, TextureHandle shadowMasks, TextureHandle prevShadowMasks, bool hasHistory)
	{
		m_DepthBuffer = depthBuffer;
		m_GBufferMotion = gbufferMotion;

		const PipelineStage computeStage = PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		builder.ReadTexture(m_Registry->GetTexture(depthBuffer), computeStage, BARRIER_ACCESS_SHADER_READ_BIT);
		builder.ReadTexture(m_Registry->GetTexture(gbufferMotion), computeStage, BARRIER_ACCESS_SHADER_READ_BIT);
		builder.ReadTexture(m_Registry->GetTexture(shadowMasksRaw), computeStage, BARRIER_ACCESS_SHADER_READ_BIT);
		builder.WriteTexture(m_Registry->GetTexture(shadowMasks), computeStage, BARRIER_ACCESS_SHADER_WRITE_BIT);

		if (hasHistory)
		{
			// A distinct resource from shadowMasks above (the previous buffered viewport slot's
			// own texture, not this tick's) - the render graph's persisted per-resource state
			// (set by that slot's own denoise write, a tick ago) is what makes this a correct,
			// genuinely cross-tick read, not just a same-tick declaration.
			builder.ReadTexture(m_Registry->GetTexture(prevShadowMasks), computeStage, BARRIER_ACCESS_SHADER_READ_BIT);
		}
	}

	void ShadowDenoisePass::Execute(CommandList& cmdList, const LocalArray<uint, RenderConstants::c_MaxShadowSlots>& activeSlots,
		uint renderFrameIndex, uint prevRenderFrameIndex, bool hasHistory, uint width, uint height, uint spatialRadius)
	{
		if (activeSlots.IsEmpty())
		{
			return;
		}

		cmdList.BindComputePipeline(m_Pipeline);
		cmdList.BindDescriptorSet(m_Resources->descriptorSet, m_Pipeline);

		const uint groupsX = (width + 7) / 8;
		const uint groupsY = (height + 7) / 8;

		ShadowDenoisePushConstants pushConstants{};
		pushConstants.depthIndex = m_DepthBuffer.h.index;
		pushConstants.motionIndex = m_GBufferMotion.h.index;
		pushConstants.width = width;
		pushConstants.height = height;
		pushConstants.renderFrameIndex = renderFrameIndex;
		pushConstants.prevRenderFrameIndex = prevRenderFrameIndex;
		pushConstants.hasHistory = hasHistory ? 1u : 0u;
		pushConstants.spatialRadius = spatialRadius;

		for (uint slot : activeSlots)
		{
			pushConstants.slot = slot;
			cmdList.PushConstants(m_Pipeline, SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ShadowDenoisePushConstants), &pushConstants);
			cmdList.Dispatch(groupsX, groupsY, 1);
		}
	}
}
