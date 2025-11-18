#pragma once

#include "Core.h"
#include "RendererMacros.h"
#include "RenderAPI/RenderAPI.h"
#include "RenderAPI/AccelerationStructure.h"
#include "RenderAPI/Buffer.h"
#include "RenderAPI/Image.h"
#include "Resources/RenderResource.h"
#include "Resources/RenderBuffer.h"
#include "Resources/Texture.h"

namespace tyr
{
	enum class RenderGraphResourceType : uint8
	{
		TransferBuffer,
		RenderBuffer,
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

	struct RenderGraphResourceRegion
	{
		RenderResource* resource = nullptr;
		uint regionIndex = 0;
	};

	struct RenderGraphDependencyInput
	{
		RenderGraphResourceType type;
		bool external = false;
		// The state(s) the resource is expected to be in on input and the pipeline stage of usage
		RenderGraphResourceStateInfo stateInfo;
		union
		{
			// Required for external resources
			RenderGraphResourceRegion resourceRegion;
			// Required for internal resources
			RenderGraphResourceCreateInfo createInfo;
		};
	};

	struct RenderGraphDependencyOutput
	{
		union
		{
			// Required for external resources
			RenderGraphResourceRegion resourceRegion;
			// Required for internal resources
			RenderGraphResourceCreateInfo createInfo;
		};
		RenderGraphResourceType type;
		bool external = true;
	};

	struct RenderGraphResource
	{
		RenderGraphResourceRegion resourceRegion;
		RenderGraphResourceType type;
		RenderGraphResourceStateInfo stateInfo;
		bool external = false;
	};
}