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

	void GeometryPass::CreateRenderGraphDependencies(RGVector<RenderGraphDependencyInput>& inputs, RGVector<RenderGraphDependencyOutput>& outputs)
	{
		const SceneData& sceneData = m_Scene->GetSceneData();

	}
}