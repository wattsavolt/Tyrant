#pragma once

#include "Core.h"
#include "RendererMacros.h"
#include "RenderAPI/RenderAPI.h"
#include "RenderAPI/AccelerationStructure.h"
#include "RenderAPI/Buffer.h"
#include "RenderAPI/Image.h"
#include "Resources/RenderResource.h"
#include "Resources/TransferBuffer.h"
#include "Resources/GpuBuffer.h"
#include "Resources/Texture.h"

namespace tyr
{
	enum class RenderGraphResourceType
	{
		TransferBuffer,
		GpuBuffer,
		Texture,
		Attachment
	};

	struct RenderGraphResourceStateInfo
	{
		BarrierAccess access = BARRIER_ACCESS_NONE;
		PipelineStage stage = PIPELINE_STAGE_NONE;
		// images only
		ImageLayout imageLayout = IMAGE_LAYOUT_UNKNOWN;
	};

	struct RenderGraphResourceCreateInfo
	{
		union
		{
			TransferBufferDesc transferBufferDesc;
			GpuBufferDesc gpuBufferDesc;
			TextureDesc textureDesc;
		};
	};

	struct RenderGraphDependencyInput
	{
		RenderGraphResourceType type;
		bool external = false;
		// The state(s) the resource is expected to be in on input and the pipeline stage of usage
		RenderGraphResourceStateInfo stateInfo;
		union
		{
			// Only required for external resources
			RenderResource* resource = nullptr;
			// Only required for internal resources
			RenderGraphResourceCreateInfo createInfo;
		};
	};

	struct RenderGraphDependencyOutput
	{
		RenderGraphResourceType type;
		bool external = false;
		union
		{
			// Only required for external resources
			RenderResource* resource = nullptr;
			// Only required for internal resources
			RenderGraphResourceCreateInfo createInfo;
		};
	};

	struct RenderGraphResource
	{
		RenderGraphResourceType type;
		RenderGraphResourceStateInfo stateInfo;
		bool external = false;
		RenderResource* resource = nullptr;
	};
}