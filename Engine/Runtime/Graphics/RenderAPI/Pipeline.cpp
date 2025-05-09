

#include "Pipeline.h"

namespace tyr
{
	RenderPass::RenderPass(const RenderPassDesc& desc)
		: m_Desc(desc)
	{

	}

	GraphicsPipeline::GraphicsPipeline(const GraphicsPipelineDesc& desc)
		: m_Desc(desc)
	{
		// Create dynamic state infos
		bool viewPortDynamicState = false;
		bool scissorsDynamicState = false;
		for (DynamicState state : desc.dynamicStates)
		{
			if (state == DynamicState::Viewport)
			{
				viewPortDynamicState = true;
			}
			else if (state == DynamicState::Scissor)
			{
				scissorsDynamicState = true;
			}
		}
		// Ensure viewport and scissor can be dynamic if not provided.
		if (!viewPortDynamicState)
		{
			m_Desc.dynamicStates.Add(DynamicState::Viewport);
		}
		if (!scissorsDynamicState)
		{
			m_Desc.dynamicStates.Add(DynamicState::Scissor);
		}
	}

	ComputePipeline::ComputePipeline(const ComputePipelineDesc& desc)
		: m_Desc(desc)
	{
	}

	RayTracingPipeline::RayTracingPipeline(const RayTracingPipelineDesc& desc)
		: m_Desc(desc)
	{
	}
}