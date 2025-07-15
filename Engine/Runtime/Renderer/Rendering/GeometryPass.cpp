#include "GeometryPass.h"

namespace tyr
{
	GeometryPass::GeometryPass(GeometryPassParams params)
		: ShaderPass(params.scene)
	{
	
	}

	GeometryPass::~GeometryPass()
	{
	
	}

	void GeometryPass::CreateRenderGraphDependencies(RGArray<RenderGraphDependencyInput>& inputs, RGArray<RenderGraphDependencyOutput>& outputs)
	{
		const SceneData& sceneData = m_Scene->GetSceneData();

	}
}