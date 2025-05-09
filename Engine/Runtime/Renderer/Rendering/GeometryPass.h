#pragma once

#include "Pass.h"

namespace tyr
{
	
	struct GeometryPassParams
	{
		Scene* scene;
	};

	class GraphicsPipeline;

	// A render pass
	class GeometryPass final : public ShaderPass
	{
	public:
		// The scene passed can be nullptr if this pass instance is needed by more than one scene
		GeometryPass(GeometryPassParams params);
		~GeometryPass();

		void CreateRenderGraphDependencies(RGVector<RenderGraphDependencyInput>& inputs, RGVector<RenderGraphDependencyOutput>& outputs);

	private:
		Ref<GraphicsPipeline> m_Pipeline;
		Scene* m_Scene;
		
	};
	
}