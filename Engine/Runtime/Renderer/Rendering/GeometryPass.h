#pragma once

#include "Core.h"
#include "RendererMacros.h"
#include "RenderAPI/Pipeline.h"
#include "RenderAPI/DescriptorSet.h"

namespace tyr
{
	class Device;
	class CommandList;
	class RenderRegistry;
	class RenderGraphBuilder;
	struct Scene;
	struct RenderResources;
	struct RenderFrame;

	struct GeometryPassArgs
	{
		Device* device;
		RenderRegistry* registry;
		RenderResources* resources;
		Scene* scene;
		GraphicsPipelineHandle pipeline;
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
		void Setup(RenderGraphBuilder& builder, const RenderFrame& renderFrame);
		void Execute(CommandList& cmdList);

	private:
		Device* m_Device;
		RenderRegistry* m_Registry;
		RenderResources* m_Resources;
		Scene* m_Scene;
		GraphicsPipelineHandle m_Pipeline;
		DescriptorSetHandle m_DescriptorSet;
	};

}