#include "GUIPass.h"
#include "RenderAPI/CommandList.h"
#include "Rendering/RenderFrame.h"
#include "Rendering/RenderRegistry.h"
#include "Rendering/RenderGraphBuilder.h"
#include "RenderResource/RenderBuffer.h"

namespace tyr
{
	// Matches PushConstants in GUIVS.hlsl/GUIPS.hlsl byte for byte.
	struct GUIPushConstants
	{
		float scale[2];
		float translate[2];
		uint textureIndex;
	};

	GUIPass::GUIPass(const GUIPassArgs& args)
	{
		Recreate(args);
	}

	GUIPass::~GUIPass()
	{
	}

	void GUIPass::Recreate(const GUIPassArgs& args)
	{
		m_Registry = args.registry;
		m_Pipeline = args.pipeline;
		m_DescriptorSet = args.descriptorSet;
		m_VertexBuffer = args.vertexBuffer;
		m_IndexBuffer = args.indexBuffer;
		m_RenderFrame = args.renderFrame;
	}

	void GUIPass::Setup(RenderGraphBuilder& builder)
	{
		const PipelineStage guiPipelineStages = static_cast<PipelineStage>(PIPELINE_STAGE_VERTEX_SHADER_BIT | PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		builder.ReadBuffer(m_Registry->GetBuffer(m_VertexBuffer), guiPipelineStages, BARRIER_ACCESS_SHADER_READ_BIT);
		builder.ReadBuffer(m_Registry->GetBuffer(m_IndexBuffer), guiPipelineStages, BARRIER_ACCESS_SHADER_READ_BIT);
	}

	void GUIPass::Execute(CommandList& cmdList)
	{
		if (m_RenderFrame->guiDrawData.Size() == 0)
		{
			return;
		}

		// No BindVertexBuffers - the vertex shader pulls its own vertex out of a StructuredBuffer
		// binding instead of using fixed-function vertex input, so the vertex buffer only ever
		// needs to be bound into the descriptor set (already done once, at creation).
		const BufferHandle indexBuffer = m_Registry->GetBuffer(m_IndexBuffer).buffer;

		cmdList.BindGraphicsPipeline(m_Pipeline);
		cmdList.BindDescriptorSet(m_DescriptorSet, m_Pipeline);
		cmdList.BindIndexBuffer(indexBuffer, 0);

		const ShaderStage guiPipelineStages = static_cast<ShaderStage>(SHADER_STAGE_VERTEX_BIT | SHADER_STAGE_FRAGMENT_BIT);

		for (const GUIDrawSubmission& submission : m_RenderFrame->guiDrawData)
		{
			GUIPushConstants pushConstants;
			pushConstants.scale[0] = 2.0f / submission.displaySize.x;
			pushConstants.translate[0] = -1.0f;
			// Y is flipped relative to the usual Vulkan ImGui backend maths (scale=+2/h,
			// translate=-1) - CommandList::SetViewport flips the viewport for every pipeline to
			// match this engine's DirectX-style Y-up 3D conventions, so ImGui's top-left-origin,
			// Y-down screen coordinates need the opposite sign here to land the right way up.
			pushConstants.scale[1] = -2.0f / submission.displaySize.y;
			pushConstants.translate[1] = 1.0f;

			for (const GUIDrawCommand& command : submission.commands)
			{
				GraphicsRect scissor;
				scissor.offset.x = (int)command.clipMinX;
				scissor.offset.y = (int)command.clipMinY;
				scissor.extents.width = command.clipMaxX - command.clipMinX;
				scissor.extents.height = command.clipMaxY - command.clipMinY;
				cmdList.SetScissor(&scissor, 1);

				pushConstants.textureIndex = command.textureIndex;
				cmdList.PushConstants(m_Pipeline, guiPipelineStages, 0, sizeof(GUIPushConstants), &pushConstants);

				cmdList.DrawIndexed(command.indexCount, 1, submission.indexOffset + command.indexOffset, (int)submission.vertexOffset, 0);
			}
		}
	}
}
