#pragma once

#include "Core.h"
#include "RendererMacros.h"
#include "RenderAPI/Pipeline.h"
#include "RenderAPI/DescriptorSet.h"
#include "RenderBase/RenderHandles.h"

namespace tyr
{
	class Device;
	class CommandList;
	class RenderRegistry;
	class RenderGraphBuilder;
	struct Scene;
	struct RenderResources;
	struct RenderFrame;
	struct ShaderCullPushConstants;

	struct GeometryPassArgs
	{
		Device* device;
		RenderRegistry* registry;
		RenderResources* resources;
		Scene* scene;
		GraphicsPipelineHandle earlyPipeline;
		GraphicsPipelineHandle latePipeline;
		GraphicsPipelineHandle earlyMaskedPipeline;
		GraphicsPipelineHandle lateMaskedPipeline;
		GraphicsPipelineHandle translucentPipeline;
		DescriptorSetHandle descriptorSet;
	};

	class GeometryPass final : public INonCopyable
	{
	public:
		GeometryPass(const GeometryPassArgs& args);
		~GeometryPass();

		void Recreate(const GeometryPassArgs& args);

		// Also declares reads for this tick's material texture uploads - GeometryPass is the
		// sole intended consumer of material-flagged textures, so this is a real, exact
		// dependency even though GPU-driven culling means it can't be narrowed any further
		// than "this whole category". GUI texture uploads are unrelated and not declared here.
		// The late pass also reads the Hi-Z its task shader tests meshlets against.
		void Setup(RenderGraphBuilder& builder, const RenderFrame& renderFrame, bool late, RenderBufferHandle hizBuffer);
		void Execute(CommandList& cmdList, bool late, const ShaderCullPushConstants& pushConstants);
		// Draws the translucent list the late culling pass made.
		void ExecuteTranslucent(CommandList& cmdList, const ShaderCullPushConstants& pushConstants);

	private:
		void DrawList(CommandList& cmdList, GraphicsPipelineHandle pipeline, uint drawList, const ShaderCullPushConstants& pushConstants, ShaderStage pushConstantStages);

		Device* m_Device;
		RenderRegistry* m_Registry;
		RenderResources* m_Resources;
		Scene* m_Scene;
		GraphicsPipelineHandle m_EarlyPipeline;
		GraphicsPipelineHandle m_LatePipeline;
		GraphicsPipelineHandle m_EarlyMaskedPipeline;
		GraphicsPipelineHandle m_LateMaskedPipeline;
		GraphicsPipelineHandle m_TranslucentPipeline;
		DescriptorSetHandle m_DescriptorSet;
	};

}