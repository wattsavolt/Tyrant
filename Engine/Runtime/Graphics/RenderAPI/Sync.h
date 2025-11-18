#pragma once

#include "GraphicsBase.h"
#include "RenderAPITypes.h"
#include "Buffer.h"
#include "Image.h"

namespace tyr
{
	using FenceHandle = ResourceHandle;
	using SemaphoreHandle = ResourceHandle;
	using EventHandle = ResourceHandle;

	// Memory barrier in vulkan
	struct PipelineBarrier
	{
		BarrierAccess srcAccess;
		BarrierAccess dstAccess;
		// Stage only needed for barrier2 in vulkan
		PipelineStage srcStage;
		PipelineStage dstStage;
	};

	// Specifies whole size in vulkan
	#define TYR_BUFFER_BARRIER_WHOLE_SIZE ~0ULL

	struct BufferBarrier
	{
		BufferHandle buffer;
		size_t offset = 0;
		size_t size = ~0ULL; 
		uint srcQueueFamilyIndex = QUEUE_FAMILY_IGNORED;
		BarrierAccess srcAccess = BarrierAccess::BARRIER_ACCESS_NONE;
		BarrierAccess dstAccess = BarrierAccess::BARRIER_ACCESS_NONE;
		PipelineStage srcStage = PIPELINE_STAGE_NONE;
		PipelineStage dstStage = PIPELINE_STAGE_NONE;
	};

	struct ImageBarrier
	{
		SubresourceRange subresourceRange;
		ImageHandle image;
		uint srcQueueFamilyIndex = QUEUE_FAMILY_IGNORED;
		BarrierAccess srcAccess;
		BarrierAccess dstAccess;
		ImageLayout srcLayout = IMAGE_LAYOUT_UNKNOWN;
		ImageLayout dstLayout;
		PipelineStage srcStage;
		PipelineStage dstStage;
	};

	struct FenceDesc
	{
#if !TYR_FINAL
		GDebugString debugName;
#endif
		uint64 timeout = 5; // nanoseconds
		bool signalled = false;
	};

	struct SemaphoreDesc
	{
#if !TYR_FINAL
		GDebugString debugName;
#endif
		SemaphoreType type = SemaphoreType::Binary;
	};

	struct EventDesc
	{
#if !TYR_FINAL
		GDebugString debugName;
#endif
	};
}
