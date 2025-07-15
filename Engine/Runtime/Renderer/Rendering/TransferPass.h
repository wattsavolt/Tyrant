#pragma once

#include "Pass.h"

namespace tyr
{
	
	// A pass that executes the transfer of data from the cpu to gpu
	class TransferPass final : public Pass
	{
	public:
		// The scene passed can be nullptr if this pass instance is needed by more than one scene
		TransferPass();
		~TransferPass();

		void CreateRenderGraphDependencies(RGArray<RenderGraphDependencyInput>& inputs, RGArray<RenderGraphDependencyOutput>& outputs) override;
	};
	
}