#pragma once

#include "GraphicsBase.h"
#include "RenderAPITypes.h"
#include "RenderAPI/Sync.h"
#include "Threading/ThreadTypes.h"

namespace tyr
{
	class CommandList;
	class Device;

	struct CommandQueueExecuteArgs
	{
		CommandList** commandLists = nullptr;
		SemaphoreHandle* waitSemaphores = nullptr;
		uint64* waitValues = nullptr;
		PipelineStage* waitDstPipelineStages = nullptr;
		SemaphoreHandle* signalSemaphores = nullptr;
		uint64* signalValues = nullptr;
		uint commandListCount = 0;
		uint waitSemaphoreCount = 0;
		uint waitValueCount = 0;
		uint waitDstPipelineStageCount = 0;
		uint signalSemaphoreCount = 0;
		uint signalValueCount = 0;
	};

	/// Class repesenting a command buffer
	class TYR_GRAPHICS_API CommandQueue
	{
	public:
		CommandQueue(Device& device, const GDebugString& debugName, CommandQueueType queueType);
		virtual ~CommandQueue();

		// Submits the given work, always additionally signalling this queue's own timeline
		// semaphore to a new value - returns that value. Every other wait/signal semaphore
		// in executeDescs is exactly what gets submitted alongside it.
		uint64 Execute(const CommandQueueExecuteArgs* executeDescs, uint executeDescCount, uint queueIndexu, FenceHandle fence);

		const char* GetDebugName() const { return m_DebugName.CStr(); }
		CommandQueueType GetQueueType() const { return m_QueueType; }
		SemaphoreHandle GetTimelineSemaphore() const { return m_TimelineSemaphore; }

	protected:
		Device& m_Device;
		GDebugString m_DebugName;
		CommandQueueType m_QueueType;
		SemaphoreHandle m_TimelineSemaphore;
		Atomic<uint64> m_NextTimelineValue{ 0 };
	};
}
