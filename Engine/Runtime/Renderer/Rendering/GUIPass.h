#pragma once

#include "Core.h"
#include "RendererMacros.h"
#include "RenderAPI/Pipeline.h"
#include "RenderAPI/DescriptorSet.h"
#include "RenderBase/RenderHandles.h"

namespace tyr
{
	class CommandList;
	class RenderRegistry;
	class RenderGraphBuilder;
	struct RenderFrame;

	struct GUIPassArgs
	{
		RenderRegistry* registry;
		GraphicsPipelineHandle pipeline;
		DescriptorSetHandle descriptorSet;
		RenderBufferHandle vertexBuffer;
		RenderBufferHandle indexBuffer;
	};

	// Draws whatever immediate-mode UI draw data (editor chrome, in-game HUD/menu) was
	// submitted for this frame via RendererAPI::SubmitGUIDrawData.
	class GUIPass final : public INonCopyable
	{
	public:
		GUIPass(const GUIPassArgs& args);
		~GUIPass();

		void Recreate(const GUIPassArgs& args);

		void Setup(RenderGraphBuilder& builder);
		// renderFrame/renderFrameIndex are taken as parameters rather than stored on this
		// object - it's a single long-lived instance reused every tick, so caching per-tick
		// state here risks it going stale if anything mutates this object in between.
		void Execute(CommandList& cmdList, const RenderFrame& renderFrame, uint renderFrameIndex);

	private:
		RenderRegistry* m_Registry;
		GraphicsPipelineHandle m_Pipeline;
		DescriptorSetHandle m_DescriptorSet;
		RenderBufferHandle m_VertexBuffer;
		RenderBufferHandle m_IndexBuffer;
	};
}
