#pragma once

#include "Pass.h"
#include "RenderAPI/Pipeline.h"

namespace tyr
{
	struct GeometryPassParams
	{
		Scene* scene;
	};

	// A render pass
	class GeometryPass final : public Pass
	{
	public:
		// The scene passed can be nullptr if this pass instance is needed by more than one scene
		GeometryPass(GeometryPassParams params);
		~GeometryPass();

		void CreateRenderGraphDependencies(RGArray<RenderGraphDependencyInput>& inputs, RGArray<RenderGraphDependencyOutput>& outputs);

	private:
		GraphicsPipelineHandle m_Pipeline;
		Scene* m_Scene;
	};
	
}