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
		const GraphicsPipelineHandle pipeline = late ? m_LatePipeline : m_EarlyPipeline;
		cmdList.BindGraphicsPipeline(pipeline);
		cmdList.BindDescriptorSet(m_DescriptorSet, pipeline);

		if (!m_Scene)
		{
			return;
		}

		cmdList.PushConstants(pipeline, SHADER_STAGE_TASK_BIT, 0, sizeof(pushConstants), &pushConstants);

		// GPU-driven culling has already compacted this pass's draws into its own half of
		// indirectDrawCommandBuffer/meshDrawBuffer and written how many into its own entry of
		// drawCountBuffer, so one indirect multi-draw call covers every instance.
		const BufferHandle indirectBuffer = m_Registry->GetBuffer(m_Resources->indirectDrawCommandBuffer).buffer;
		const BufferHandle countBuffer = m_Registry->GetBuffer(m_Resources->drawCountBuffer).buffer;
		// Matches VkDrawMeshTasksIndirectCommandEXT (3x uint32).
		constexpr uint stride = sizeof(uint) * 3;
		const uint passIndex = late ? 1 : 0;
		cmdList.DrawMeshTasksIndirectCount(indirectBuffer, (size_t)passIndex * stride * RenderConstants::c_MaxMeshInstances,
			countBuffer, passIndex * sizeof(uint), RenderConstants::c_MaxMeshInstances, stride);
	}
}