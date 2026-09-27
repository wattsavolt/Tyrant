#include "GeometryPass.h"
#include "RenderAPI/Device.h"
#include "RenderAPI/CommandList.h"
#include "Rendering/Scene.h"
#include "Rendering/RenderRegistry.h"
#include "Rendering/RenderAllocationManager.h"
#include "Rendering/RenderGraphBuilder.h"
#include "Rendering/RenderResources.h"
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
		m_AllocManager = args.allocManager;
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
	}

	void GeometryPass::Execute(CommandList& cmdList)
	{
		cmdList.BindGraphicsPipeline(m_Pipeline);
		cmdList.BindDescriptorSet(m_DescriptorSet, m_Pipeline);

		if (!m_Scene)
		{
			return;
		}

		const ShaderStage meshPipelineStages = static_cast<ShaderStage>(SHADER_STAGE_TASK_BIT | SHADER_STAGE_MESH_BIT | SHADER_STAGE_FRAGMENT_BIT);

		for (MeshInstanceHandle handle : m_Scene->content.meshInstances)
		{
			const MeshInstance& instance = m_Registry->GetMeshInstance(handle);
			const Mesh& mesh = m_Registry->GetMesh(instance.info.mesh);
			if (mesh.lodCount == 0)
			{
				continue;
			}

			// LOD selection isn't implemented yet - always use LOD 0.
			const MeshLODAllocInfo& lodAlloc = m_AllocManager->GetMeshLODAllocInfo(mesh.lodOffset);
			const uint meshletCount = (uint)(lodAlloc.meshletBufferAllocation.size / sizeof(ShaderMeshlet));
			if (meshletCount == 0)
			{
				continue;
			}

			const uint meshInstanceIndex = handle.h.index;
			cmdList.PushConstants(m_Pipeline, meshPipelineStages, 0, sizeof(uint), &meshInstanceIndex);
			cmdList.DrawMeshTasks(meshletCount, 1, 1);
		}
	}
}
