
#pragma once

#include "GraphicsBase.h"
#include "RenderAPITypes.h"
#include "Buffer.h"
#include "Image.h"

namespace tyr
{
	// Memory barrier in vulkan
	struct PipelineBarrier
	{
		BarrierAccess srcAccess;
		BarrierAccess dstAccess;
		// Stage only needed for barrier2 in vulkan
		PipelineStage srcStage;
		PipelineStage dstStage;
	};

	struct BufferBarrier
	{
		Buffer* buffer;
		size_t offset = 0;
		uint srcQueueFamilyIndex = QUEUE_FAMILY_IGNORED;
		BarrierAccess srcAccess = BarrierAccess::BARRIER_ACCESS_NONE;
		BarrierAccess dstAccess = BarrierAccess::BARRIER_ACCESS_NONE;
		PipelineStage srcStage = PIPELINE_STAGE_NONE;
		PipelineStage dstStage = PIPELINE_STAGE_NONE;
	};

	struct ImageBarrier
	{
		SubresourceRange subresourceRange;
		Image* image;
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
		String debugName;
		uint64 timeout = 5; // nanoseconds
		bool signalled = false;
	};

	/// Class repesenting a fence for CPU-GPU synchronization
	class TYR_GRAPHICS_EXPORT Fence
	{
	public:
		Fence(const FenceDesc& desc);
		virtual ~Fence() = default;

		virtual void Reset() = 0;
		virtual bool GetStatus() const = 0;
		virtual bool Wait() = 0;

		const FenceDesc& GetDesc() const { return m_Desc; }

	protected:
		FenceDesc m_Desc;
	};

	struct SemaphoreDesc
	{
		String debugName;
		SemaphoreType type = SemaphoreType::Binary;
	};

	/// Class repesenting a fence for GPU-GPU synchronization
	class TYR_GRAPHICS_EXPORT Semaphore
	{
	public:
		Semaphore(const SemaphoreDesc& desc);
		virtual ~Semaphore() = default;

		virtual void Signal(uint64 value) = 0;
		virtual uint64 GetValue() const = 0;
		// timout in nanoseconds
		virtual bool Wait(uint64 value, uint64 timeout) = 0;

		const SemaphoreDesc& GetDesc() const { return m_Desc; }

	protected:
		SemaphoreDesc m_Desc;
	};

	struct EventDesc
	{
		String debugName;
	};

	/// Class repesenting an event used to synchronize the execution of command lists
	class TYR_GRAPHICS_EXPORT Event
	{
	public:
		Event(const EventDesc& desc);
		virtual ~Event() = default;

		virtual bool Set() = 0;
		virtual void Reset() = 0;
		// Returns true if the event is set or signalled
		virtual bool IsSet() const = 0;

		const EventDesc& GetDesc() const { return m_Desc; }

	protected:
		EventDesc m_Desc;
	};
}
