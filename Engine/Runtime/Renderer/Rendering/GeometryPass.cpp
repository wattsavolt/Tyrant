#include "GeometryPass.h"
#include "RenderAPI/Device.h"
#include "RenderAPI/CommandList.h"
#include "Rendering/Scene.h"
#include "Rendering/RenderRegistry.h"
#include "Rendering/RenderGraphBuilder.h"
#include "Rendering/RenderResources.h"
#include "Rendering/RenderConstants.h"
#include "Rendering/RenderFrame.h"
#include "RenderResource/Texture.h"
#include "RenderTransfer/UploadRequest.h"
#include "Shaders/ShaderTypes.h"

namespace tyr
{
	GeometryPass::GeometryPass(const GeometryPassArgs& args)
	{
		Recreate(args);
	}

	GeometryPass::~GeometryPass()
	{

	}

	void GeometryPass::Recreate(const GeometryPassArgs& args)
	{
		m_Device = args.device;
		m_Registry = args.registry;
		m_Resources = args.resources;
		m_Scene = args.scene;
		m_EarlyPipeline = args.earlyPipeline;
		m_LatePipeline = args.latePipeline;
		m_EarlyMaskedPipeline = args.earlyMaskedPipeline;
		m_LateMaskedPipeline = args.lateMaskedPipeline;
		m_TranslucentPipeline = args.translucentPipeline;
		m_DescriptorSet = args.descriptorSet;
	}

	void GeometryPass::Setup(RenderGraphBuilder& builder, const RenderFrame& renderFrame, bool late, RenderBufferHandle hizBuffer)
	{
		const PipelineStage meshPipelineStages = static_cast<PipelineStage>(PIPELINE_STAGE_TASK_SHADER_BIT | PIPELINE_STAGE_MESH_SHADER_BIT | PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

		builder.ReadBuffer(m_Registry->GetBuffer(m_Resources->sceneInfoBuffer), meshPipelineStages, BARRIER_ACCESS_UNIFORM_READ_BIT);

		const RenderBufferHandle buffers[] = {
			m_Resources->meshBuffer, m_Resources->meshLODBuffer, m_Resources->meshletBuffer,
			m_Resources->vertexBuffer, m_Resources->indexBuffer, m_Resources->meshInstanceBuffer,
			m_Resources->materialBuffer, m_Resources->directionalLightBuffer, m_Resources->pointLightBuffer,
			m_Resources->spotLightBuffer
		};

		for (RenderBufferHandle buffer : buffers)
		{
			builder.ReadBuffer(m_Registry->GetBuffer(buffer), meshPipelineStages, BARRIER_ACCESS_SHADER_READ_BIT);
		}

		// GPU-driven culling (run just before this pass in the same render graph phase) writes
		// these three.
		builder.ReadBuffer(m_Registry->GetBuffer(m_Resources->meshDrawBuffer), meshPipelineStages, BARRIER_ACCESS_SHADER_READ_BIT);
		builder.ReadBuffer(m_Registry->GetBuffer(m_Resources->indirectDrawCommandBuffer), PIPELINE_STAGE_DRAW_INDIRECT_BIT, BARRIER_ACCESS_INDIRECT_COMBAND_READ_BIT);
		builder.ReadBuffer(m_Registry->GetBuffer(m_Resources->drawCountBuffer), PIPELINE_STAGE_DRAW_INDIRECT_BIT, BARRIER_ACCESS_INDIRECT_COMBAND_READ_BIT);

		if (late)
		{
			builder.ReadBuffer(m_Registry->GetBuffer(hizBuffer), PIPELINE_STAGE_TASK_SHADER_BIT, BARRIER_ACCESS_SHADER_READ_BIT);
		}

		for (const TextureUploadRequest& request : renderFrame.textureUploadRequests)
		{
			if (request.type == TextureUploadRequestType::Material)
			{
				builder.ReadTexture(m_Registry->GetTexture(request.dstTexture), PIPELINE_STAGE_FRAGMENT_SHADER_BIT, BARRIER_ACCESS_SHADER_READ_BIT, IMAGE_LAYOUT_GENERAL);
			}
		}
	}

	void GeometryPass::Execute(CommandList& cmdList, bool late, const ShaderCullPushConstants& pushConstants)
	{
		const GraphicsPipelineHandle opaquePipeline = late ? m_LatePipeline : m_EarlyPipeline;
		cmdList.BindGraphicsPipeline(opaquePipeline);
		cmdList.BindDescriptorSet(m_DescriptorSet, opaquePipeline);

		if (!m_Scene)
		{
			return;
		}

		// Opaque first, so masked pixels behind it are rejected before they're shaded.
		DrawList(cmdList, opaquePipeline, late ? TYR_DRAW_LIST_LATE_OPAQUE : TYR_DRAW_LIST_EARLY_OPAQUE, pushConstants, SHADER_STAGE_TASK_BIT);

		const GraphicsPipelineHandle maskedPipeline = late ? m_LateMaskedPipeline : m_EarlyMaskedPipeline;
		cmdList.BindGraphicsPipeline(maskedPipeline);
		DrawList(cmdList, maskedPipeline, late ? TYR_DRAW_LIST_LATE_MASKED : TYR_DRAW_LIST_EARLY_MASKED, pushConstants, SHADER_STAGE_TASK_BIT);
	}

	void GeometryPass::ExecuteTranslucent(CommandList& cmdList, const ShaderCullPushConstants& pushConstants)
	{
		cmdList.BindGraphicsPipeline(m_TranslucentPipeline);
		cmdList.BindDescriptorSet(m_DescriptorSet, m_TranslucentPipeline);

		if (!m_Scene)
		{
			return;
		}

		// The pixel shader also reads which frame's ray tracing structure to trace shadows in.
		DrawList(cmdList, m_TranslucentPipeline, TYR_DRAW_LIST_TRANSLUCENT, pushConstants,
			static_cast<ShaderStage>(SHADER_STAGE_TASK_BIT | SHADER_STAGE_FRAGMENT_BIT));
	}

	void GeometryPass::DrawList(CommandList& cmdList, GraphicsPipelineHandle pipeline, uint drawList, const ShaderCullPushConstants& pushConstants, ShaderStage pushConstantStages)
	{
		cmdList.PushConstants(pipeline, pushConstantStages, 0, sizeof(pushConstants), &pushConstants);

		// GPU-driven culling has already compacted this list's draws into its own part of
		// indirectDrawCommandBuffer/meshDrawBuffer and written how many into its own entry of
		// drawCountBuffer, so one indirect multi-draw call covers every instance.
		const BufferHandle indirectBuffer = m_Registry->GetBuffer(m_Resources->indirectDrawCommandBuffer).buffer;
		const BufferHandle countBuffer = m_Registry->GetBuffer(m_Resources->drawCountBuffer).buffer;
		// Matches VkDrawMeshTasksIndirectCommandEXT (3x uint32).
		constexpr uint stride = sizeof(uint) * 3;
		cmdList.DrawMeshTasksIndirectCount(indirectBuffer, (size_t)drawList * stride * RenderConstants::c_MaxMeshInstances,
			countBuffer, drawList * sizeof(uint), RenderConstants::c_MaxMeshInstances, stride);
	}
}