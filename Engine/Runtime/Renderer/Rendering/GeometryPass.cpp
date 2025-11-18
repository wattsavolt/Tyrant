#include "GeometryPass.h"

namespace tyr
{
	GeometryPass::GeometryPass(GeometryPassParams params)
		: Pass(params.scene)
	{
	
	}

	GeometryPass::~GeometryPass()
	{
	
	}

	void GeometryPass::CreateRenderGraphDependencies(RGArray<RenderGraphDependencyInput>& inputs, RGArray<RenderGraphDependencyOutput>& outputs)
	{
		

	}
}