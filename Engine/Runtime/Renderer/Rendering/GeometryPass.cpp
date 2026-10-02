#include "GeometryPass.h"
#include "RenderAPI/Device.h"
#include "RenderAPI/CommandList.h"
#include "Rendering/Scene.h"
#include "Rendering/RenderRegistry.h"
#include "Rendering/RenderGraphBuilder.h"
#include "Rendering/RenderResources.h"
#include "Rendering/RenderConstants.h"
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
		m_Pipeline = args.pipeline;
		m_DescriptorSet = args.descriptorSet;
	}

	void GeometryPass::Setup(RenderGraphBuilder& builder)
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

		// GPU-driven instance culling (run just before this pass in the same render graph
		// phase) writes these three.
		builder.ReadBuffer(m_Registry->GetBuffer(m_Resources->visibleInstanceIndexBuffer), meshPipelineStages, BARRIER_ACCESS_SHADER_READ_BIT);
		builder.ReadBuffer(m_Registry->GetBuffer(m_Resources->indirectDrawCommandBuffer), PIPELINE_STAGE_DRAW_INDIRECT_BIT, BARRIER_ACCESS_INDIRECT_COMBAND_READ_BIT);
		builder.ReadBuffer(m_Registry->GetBuffer(m_Resources->drawCountBuffer), PIPELINE_STAGE_DRAW_INDIRECT_BIT, BARRIER_ACCESS_INDIRECT_COMBAND_READ_BIT);
	}

	void GeometryPass::Execute(CommandList& cmdList)
	{
		cmdList.BindGraphicsPipeline(m_Pipeline);
		cmdList.BindDescriptorSet(m_DescriptorSet, m_Pipeline);

		if (!m_Scene)
		{
			return;
		}

		// GPU-driven instance culling has already compacted every visible instance into
		// indirectDrawCommandBuffer/visibleInstanceIndexBuffer and written how many into
		// drawCountBuffer, so one indirect multi-draw call covers every instance.
		const BufferHandle indirectBuffer = m_Registry->GetBuffer(m_Resources->indirectDrawCommandBuffer).buffer;
		const BufferHandle countBuffer = m_Registry->GetBuffer(m_Resources->drawCountBuffer).buffer;
		// Matches VkDrawMeshTasksIndirectCommandEXT (3x uint32).
		constexpr uint stride = sizeof(uint) * 3;
		cmdList.DrawMeshTasksIndirectCount(indirectBuffer, 0, countBuffer, 0, RenderConstants::c_MaxMeshInstances, stride);
	}
}
