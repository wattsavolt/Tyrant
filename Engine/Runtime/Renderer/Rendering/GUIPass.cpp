#include "GUIPass.h"
#include "RenderDebug.h"
#include "RenderAPI/CommandList.h"
#include "Rendering/RenderFrame.h"
#include "Rendering/RenderRegistry.h"
#include "Rendering/RenderGraphBuilder.h"
#include "Rendering/RenderConstants.h"
#include "RenderResource/RenderBuffer.h"
#include <chrono>

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
	}

	void GUIPass::Setup(RenderGraphBuilder& builder)
	{
		// The vertex shader pulls its own vertex out of this via a StructuredBuffer binding - a
		// real shader read, at the shader stages.
		const PipelineStage guiPipelineStages = static_cast<PipelineStage>(PIPELINE_STAGE_VERTEX_SHADER_BIT | PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		builder.ReadBuffer(m_Registry->GetBuffer(m_VertexBuffer), guiPipelineStages, BARRIER_ACCESS_SHADER_READ_BIT);

		// Unlike the vertex buffer above, this one is consumed through fixed-function index
		// fetch, so it needs INDEX_READ at the index-input stage instead.
		builder.ReadBuffer(m_Registry->GetBuffer(m_IndexBuffer), PIPELINE_STAGE_INDEX_INPUT_BIT, BARRIER_ACCESS_INDEX_READ_BIT);
	}

	void GUIPass::Execute(CommandList& cmdList, const RenderFrame& renderFrame, uint renderFrameIndex)
	{
#if TYR_RENDER_DEBUG
		// A wall-clock timestamp lets a screenshot's capture time be matched back to an exact
		// renderFrameIndex/submission count.
		{
			uint totalCommands = 0;
			for (const GUIDrawSubmission& submission : renderFrame.guiDrawData)
			{
				totalCommands += submission.commands.Size();
			}
			const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			TYR_LOG_WARNING("[DBG] GUIPass::Execute: ms=%lld slot=%u submissions=%u totalCommands=%u",
				(long long)nowMs, renderFrameIndex, renderFrame.guiDrawData.Size(), totalCommands);
		}
#endif

		if (renderFrame.guiDrawData.Size() == 0)
		{
			return;
		}

		// No BindVertexBuffers - the vertex shader pulls its own vertex out of a StructuredBuffer
		// binding instead of using fixed-function vertex input, so the vertex buffer only ever
		// needs to be bound into the descriptor set (already done once, at creation).
		const BufferHandle indexBuffer = m_Registry->GetBuffer(m_IndexBuffer).buffer;

		// Both buffers are one physical buffer, c_BufferedFrameCount slots big. The vertex
		// buffer has no bound range of its own (read via a whole-buffer StructuredBuffer
		// descriptor), so its slot base folds into vertexOffset instead.
		const size_t indexBase = renderFrameIndex * RenderConstants::c_GUIIndexBufferSize;
		const int vertexBase = (int)(renderFrameIndex * (RenderConstants::c_GUIVertexBufferSize / sizeof(GUIVertex)));

		cmdList.BindGraphicsPipeline(m_Pipeline);
		cmdList.BindDescriptorSet(m_DescriptorSet, m_Pipeline);
		cmdList.BindIndexBuffer(indexBuffer, indexBase);

		const ShaderStage guiPipelineStages = static_cast<ShaderStage>(SHADER_STAGE_VERTEX_BIT | SHADER_STAGE_FRAGMENT_BIT);

		for (const GUIDrawSubmission& submission : renderFrame.guiDrawData)
		{
			// A zero (or garbage) display size would make scale = +-INF below, and any vertex
			// whose local position happens to be exactly 0 on that axis would compute a NaN
			// clip-space coordinate - which makes the rasterizer discard the whole submission.
			if (submission.displaySize.x <= 0.0f || submission.displaySize.y <= 0.0f)
			{
				continue;
			}

			GUIPushConstants pushConstants;
			pushConstants.scale[0] = 2.0f / submission.displaySize.x;
			pushConstants.translate[0] = -1.0f;
			// Y is flipped relative to the usual Vulkan ImGui maths - the viewport is flipped for
			// every pipeline to match this engine's DirectX-style Y-up 3D conventions, so ImGui's
			// Y-down coordinates need the opposite sign here.
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

				cmdList.DrawIndexed(command.indexCount, 1, submission.indexOffset + command.indexOffset, vertexBase + (int)submission.vertexOffset, 0);
			}
		}
	}
}
