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
		// Shader binding table - one GPU buffer holding every shader group's handle, laid out
		// per the four regions below (see Device::CreateRayTracingPipeline).
		BufferHandle sbtBuffer;
		VkStridedDeviceAddressRegionKHR raygenRegion{};
		VkStridedDeviceAddressRegionKHR missRegion{};
		VkStridedDeviceAddressRegionKHR hitRegion{};
		VkStridedDeviceAddressRegionKHR callableRegion{};
	};
}
