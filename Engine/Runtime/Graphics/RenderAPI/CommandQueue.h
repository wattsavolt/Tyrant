#pragma once

#include "GraphicsBase.h"
#include "RenderAPITypes.h"
#include "RenderAPI/Sync.h"

namespace tyr
{
	class CommandList;

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
		CommandQueue(const GDebugString& debugName, CommandQueueType queueType);
		virtual ~CommandQueue() = default;
		
		void Execute(const CommandQueueExecuteArgs* executeDescs, uint executeDescCount, uint queueIndexu, FenceHandle fence);

		const char* GetDebugName() const { return m_DebugName.CStr(); }
		CommandQueueType GetQueueType() const { return m_QueueType; }


	protected:
		GDebugString m_DebugName;
		CommandQueueType m_QueueType;
	};
}
