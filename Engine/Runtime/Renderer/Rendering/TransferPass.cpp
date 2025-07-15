#include "TransferPass.h"

namespace tyr
{
	TransferPass::TransferPass()
		: Pass(Scene::c_InvalidID)
	{
	
	}

	TransferPass::~TransferPass()
	{
	
	}

	void TransferPass::CreateRenderGraphDependencies(RGArray<RenderGraphDependencyInput>& inputs, RGArray<RenderGraphDependencyOutput>& outputs)
	{

	}
}