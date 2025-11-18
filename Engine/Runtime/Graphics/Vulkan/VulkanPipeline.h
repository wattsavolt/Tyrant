#pragma once

#include "RenderAPI/Pipeline.h"
#include "VulkanCommon.h"

namespace tyr
{
	struct RenderPass
	{
		VkRenderPass renderPass;
	};

	struct GraphicsPipeline
	{
		VkPipelineLayout pipelineLayout;
		VkPipeline pipeline;
	};

	struct ComputePipeline
	{
		VkPipelineLayout pipelineLayout;
		VkPipeline pipeline;
	};

	struct RayTracingPipeline
	{
		VkPipelineLayout pipelineLayout;
		VkPipeline pipeline;
	};
}
