#pragma once

#include "Core.h"
#include "RendererMacros.h"
#include "Containers/MPSCRingBuffer.h"
#include "RenderConstants.h"

namespace tyr
{
	class Device;
	class SwapChain;
	class CommandQueue;
	class CommandList;
	class Semaphore;

	struct RenderSubmissionThreadArgs
	{
		Device* device;
		CommandQueue* graphicsQueue;
		CommandQueue* computeQueue;
		CommandQueue* transferQueue;
	};

	struct RenderSubmissionRequest
	{
		// Maybe increase later
		static constexpr uint c_MaxCommandLists = 8;
		static constexpr uint c_MaxWaitSemaphores = 4;
		static constexpr uint c_MaxSignalSemaphores = 4;
	
		CommandQueueType queueType;
		LocalArray<CommandList*, c_MaxCommandLists> commandLists;
		LocalArray<SemaphoreHandle, c_MaxWaitSemaphores> waitSemaphores;
		LocalArray<uint64, c_MaxWaitSemaphores> waitValues;
		LocalArray<PipelineStage, c_MaxWaitSemaphores> waitDstPipelineStages;
		LocalArray<SemaphoreHandle, c_MaxSignalSemaphores> signalSemaphores;
		LocalArray<uint64, c_MaxSignalSemaphores> signalValues;
	};

	struct RenderPresentRequest
	{
		SwapChain* swapChain;
		SemaphoreHandle waitSemaphore;
		uint imageIndex;
	};

	struct RenderNotification
	{
		bool resizeRequired = false;
	};

	class RenderSubmissionThread final : INonCopyable
	{
	public:
		RenderSubmissionThread(const RenderSubmissionThreadArgs& args);
		~RenderSubmissionThread();
		void Run();
		void EnqueueRenderSubmissionRequest(const RenderSubmissionRequest& request);
		void EnqueueRenderPresentRequest(const RenderPresentRequest& request);
		void EnqueueRenderNotification(const RenderNotification& notification);

	private:
		// Operates on its own thread
		void ProcessRequests();
		Device* m_Device;
		CommandQueue* m_GraphicsQueue;
		CommandQueue* m_ComputeQueue;
		CommandQueue* m_TransferQueue;
	};
	
}