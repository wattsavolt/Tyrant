#include "RenderSubmissionThread.h"
#include "RenderDebug.h"
#include "RenderAPI/Device.h"
#include "RenderAPI/SwapChain.h"
#include "RenderAPI/CommandQueue.h"
#include "RenderAPI/CommandList.h"
#include "RenderAPI/Sync.h"

namespace tyr
{
	RenderSubmissionThread::RenderSubmissionThread(const RenderSubmissionThreadArgs& args)
		: m_Device(args.device)
		, m_GraphicsQueue(args.graphicsQueue)
		, m_ComputeQueue(args.computeQueue)
		, m_TransferQueue(args.transferQueue)
	{
		m_Thread = Thread(&RenderSubmissionThread::Run, this);
	}

	RenderSubmissionThread::~RenderSubmissionThread()
	{
		{
			LockGuard guard(m_Mutex);
			m_Stop.store(true, std::memory_order_relaxed);
		}
		m_CV.notify_one();

		if (m_Thread.joinable())
		{
			m_Thread.join();
		}
	}

	void RenderSubmissionThread::EnqueueRenderSubmissionRequest(const RenderSubmissionRequest& request)
	{
		const bool enqueued = m_SubmissionQueue.Enqueue(request);
		TYR_ASSERT(enqueued);
		(void)enqueued;

		m_CV.notify_one();
	}

	void RenderSubmissionThread::EnqueueRenderPresentRequest(const RenderPresentRequest& request)
	{
		const bool enqueued = m_PresentQueue.Enqueue(request);
		TYR_ASSERT(enqueued);
		(void)enqueued;

		m_CV.notify_one();
	}

	void RenderSubmissionThread::EnqueueRenderNotification(const RenderNotification& notification)
	{
		const bool enqueued = m_NotificationQueue.Enqueue(notification);
		TYR_ASSERT(enqueued);
		(void)enqueued;

		m_CV.notify_one();
	}

	void RenderSubmissionThread::EnqueueRenderAcquireRequest(const RenderAcquireRequest& request)
	{
		const bool enqueued = m_AcquireQueue.Enqueue(request);
		TYR_ASSERT(enqueued);
		(void)enqueued;

		m_CV.notify_one();
	}

	Optional<RenderFrameCompletion> RenderSubmissionThread::DequeueFrameCompletion()
	{
		return m_CompletionQueue.Dequeue();
	}

	Optional<RenderAcquireResult> RenderSubmissionThread::DequeueAcquireResult()
	{
		return m_AcquireResultQueue.Dequeue();
	}

	Optional<RenderNotification> RenderSubmissionThread::DequeueRenderNotification()
	{
		return m_NotificationQueue.Dequeue();
	}

	uint64 RenderSubmissionThread::GetLastPresentedFrame() const
	{
		return m_LastPresentedFrame.load(std::memory_order_acquire);
	}

	void RenderSubmissionThread::Run()
	{
		while (!m_Stop.load(std::memory_order_relaxed))
		{
			if (ProcessRequests())
			{
				continue;
			}

			Lock lock(m_Mutex);
			m_CV.wait_for(lock, std::chrono::microseconds(200), [this] { return m_Stop.load(std::memory_order_relaxed); });
		}

		// Drain whatever's left so nothing queued right before shutdown is silently dropped.
		ProcessRequests();
	}

	bool RenderSubmissionThread::ProcessRequests()
	{
		bool processedAny = false;

		// Processed first, ahead of submissions/presents, since RenderAsync is blocked
		// spin-waiting on the result - it needs the acquired image index before it can even
		// start recording.
		while (Optional<RenderAcquireRequest> request = m_AcquireQueue.Dequeue())
		{
			bool valid = false;
			bool resizeNeeded = false;
			const uint imageIndex = request->swapChain->AcquireNextImage(request->semaphore, valid, resizeNeeded);

			const bool resultEnqueued = m_AcquireResultQueue.Enqueue({ imageIndex, valid, resizeNeeded });
			TYR_ASSERT(resultEnqueued);
			(void)resultEnqueued;

			if (resizeNeeded)
			{
				const bool notified = m_NotificationQueue.Enqueue({ request->window, true });
				TYR_ASSERT(notified);
				(void)notified;
			}

			processedAny = true;
		}

		while (Optional<RenderSubmissionRequest> request = m_SubmissionQueue.Dequeue())
		{
			CommandQueue* queue = m_GraphicsQueue;
			switch (request->queueType)
			{
			case CommandQueueType::CQ_COMPUTE:
				queue = m_ComputeQueue;
				break;
			case CommandQueueType::CQ_TRANSFER:
				queue = m_TransferQueue;
				break;
			default:
				break;
			}

			CommandQueueExecuteArgs executeArgs{};
			executeArgs.commandLists = request->commandLists.Data();
			executeArgs.commandListCount = request->commandLists.Size();
			executeArgs.waitSemaphores = request->waitSemaphores.Data();
			executeArgs.waitValues = request->waitValues.Data();
			executeArgs.waitValueCount = request->waitValues.Size();
			executeArgs.waitDstPipelineStages = request->waitDstPipelineStages.Data();
			executeArgs.waitSemaphoreCount = request->waitSemaphores.Size();
			executeArgs.waitDstPipelineStageCount = request->waitDstPipelineStages.Size();
			executeArgs.signalSemaphores = request->signalSemaphores.Data();
			executeArgs.signalValues = request->signalValues.Data();
			executeArgs.signalSemaphoreCount = request->signalSemaphores.Size();
			executeArgs.signalValueCount = request->signalValues.Size();

			const uint64 timelineValue = queue->Execute(&executeArgs, 1, 0, request->fence);

			if (request->reportCompletion)
			{
				const bool completionEnqueued = m_CompletionQueue.Enqueue({ request->renderFrameIndex, timelineValue, request->frameNumber });
				TYR_ASSERT(completionEnqueued);
				(void)completionEnqueued;
			}

			processedAny = true;
		}

		while (Optional<RenderPresentRequest> request = m_PresentQueue.Dequeue())
		{
			if (request->present)
			{
				bool resizeNeeded = false;
				request->swapChain->Present(request->queue, request->waitSemaphore, request->imageIndex, resizeNeeded);

#if TYR_RENDER_DEBUG
				// Logs the presented image index per frame, for cross-checking against the
				// acquired index logged elsewhere.
				TYR_LOG_WARNING("[DBG] Present: frameNumber=%llu imageIndex=%u", (unsigned long long)request->frameNumber, request->imageIndex);
#endif

				if (resizeNeeded)
				{
					const bool notified = m_NotificationQueue.Enqueue({ request->window, true });
					TYR_ASSERT(notified);
					(void)notified;
				}
			}

			// Presents are enqueued in strictly increasing frame order and this queue is FIFO, so
			// a plain monotonic store is enough. Stored even when present is false, so a frame
			// that never actually presented still counts as handled.
			m_LastPresentedFrame.store(request->frameNumber, std::memory_order_release);

			processedAny = true;
		}

		return processedAny;
	}
}