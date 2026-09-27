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
	class RenderAllocationManager;
	class RenderGraphBuilder;
	struct Scene;
	struct RenderResources;

	struct GeometryPassArgs
	{
		Device* device;
		RenderRegistry* registry;
		RenderAllocationManager* allocManager;
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

		void Setup(RenderGraphBuilder& builder);
		void Execute(CommandList& cmdList);

	private:
		Device* m_Device;
		RenderRegistry* m_Registry;
		RenderAllocationManager* m_AllocManager;
		RenderResources* m_Resources;
		Scene* m_Scene;
		GraphicsPipelineHandle m_Pipeline;
		DescriptorSetHandle m_DescriptorSet;
	};

}