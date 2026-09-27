#pragma once

#include "Core.h"
#include "RendererMacros.h"
#include "Containers/MPSCRingBuffer.h"
#include "Containers/SPSCRingBuffer.h"
#include "Threading/ThreadTypes.h"
#include "RenderAPI/Sync.h"
#include "RenderConstants.h"
#include "RenderBase/RenderHandles.h"

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
		FenceHandle fence;

		// Set for the one main per-frame graphics submission - tells this thread to report
		// the timeline value this submission actually gets back to the main thread, tagged
		// with which render frame slot it belongs to.
		bool reportCompletion = false;
		uint renderFrameIndex = 0;
		// The true, non-wrapping frame number (see Renderer::m_FrameNumber) this submission
		// belongs to - lets the main thread tell this report apart from a stale one reported
		// for an earlier use of the same renderFrameIndex slot.
		uint64 frameNumber = 0;
	};

	struct RenderFrameCompletion
	{
		uint renderFrameIndex;
		uint64 timelineValue;
		uint64 frameNumber;
	};

	struct RenderPresentRequest
	{
		SwapChain* swapChain;
		CommandQueue* queue;
		SemaphoreHandle waitSemaphore;
		uint imageIndex;
		// The true, non-wrapping frame number this present belongs to (see Renderer::m_FrameNumber).
		uint64 frameNumber = 0;
		RenderWindowHandle window;
		// False when this frame's acquire failed (no valid image) - the actual Present() call is
		// skipped, but frameNumber is still recorded as "handled" via GetLastPresentedFrame() so
		// Render()'s pacing wait isn't left waiting forever for a present that was never going to
		// happen.
		bool present = true;
	};

	// Enqueued whenever an acquire or present indicates its swap chain should be recreated
	// (VK_SUBOPTIMAL_KHR or VK_ERROR_OUT_OF_DATE_KHR). Drained and resized every tick by
	// Renderer::Render itself.
	struct RenderNotification
	{
		RenderWindowHandle window;
		bool resizeRequired = false;
	};

	// A swap chain's acquire/present calls must be externally synchronized against each other,
	// and RenderAsync (which needs the acquired image index before it can even start recording)
	// runs on a worker-pool thread while Present() is issued from this thread - so acquiring is
	// also done here, keeping every call touching a given VkSwapchainKHR on this one thread.
	struct RenderAcquireRequest
	{
		SwapChain* swapChain;
		SemaphoreHandle semaphore;
		RenderWindowHandle window;
	};

	struct RenderAcquireResult
	{
		// Only meaningful when valid is true.
		uint imageIndex;
		// False when acquisition genuinely failed (VK_ERROR_OUT_OF_DATE_KHR) - imageIndex must
		// not be used in that case.
		bool valid;
		// True when the swap chain should be recreated - covers both VK_SUBOPTIMAL_KHR (valid is
		// still true) and VK_ERROR_OUT_OF_DATE_KHR (valid is false).
		bool resizeNeeded;
	};

	// Runs on its own thread. Takes submission and present requests off a queue and sends
	// them to the graphics API.
	class RenderSubmissionThread final : INonCopyable
	{
	public:
		RenderSubmissionThread(const RenderSubmissionThreadArgs& args);
		~RenderSubmissionThread();

		// Adds a submission/present/notification/acquire request to this thread's queue. Safe
		// to call from any thread.
		void EnqueueRenderSubmissionRequest(const RenderSubmissionRequest& request);
		void EnqueueRenderPresentRequest(const RenderPresentRequest& request);
		void EnqueueRenderNotification(const RenderNotification& notification);
		void EnqueueRenderAcquireRequest(const RenderAcquireRequest& request);

		// Reads one reported frame completion, if any are waiting. Only ever called from
		// the main thread.
		Optional<RenderFrameCompletion> DequeueFrameCompletion();

		// Reads one acquire result, if it's ready yet. Only ever one request in flight at a
		// time (RenderAsync calls are serialized), so whichever result comes back next is
		// always the caller's own.
		Optional<RenderAcquireResult> DequeueAcquireResult();

		// Reads one pending notification, if any are waiting. Call in a loop to drain all of
		// them - only ever called from the main thread.
		Optional<RenderNotification> DequeueRenderNotification();

		// The highest frame number (see RenderPresentRequest::frameNumber) whose Present() call
		// this thread has issued so far. Presents are issued in strict frame order (this thread
		// drains its present queue FIFO, and frames are enqueued to it in submission order), so
		// this single value is enough to know whether any earlier frame's present has been
		// issued too - no need to track per-frame. Safe to call from any thread.
		uint64 GetLastPresentedFrame() const;

	private:
		// The loop this thread runs for its whole lifetime.
		void Run();

		// Sends every request currently queued up to the graphics API. Returns true if it
		// processed at least one request.
		bool ProcessRequests();

		static constexpr uint c_MaxPendingRequests = 64;
		static constexpr uint c_MaxPendingNotifications = 16;
		static constexpr uint c_MaxPendingCompletions = 8;
		static constexpr uint c_MaxPendingAcquires = 8;

		Device* m_Device;
		CommandQueue* m_GraphicsQueue;
		CommandQueue* m_ComputeQueue;
		CommandQueue* m_TransferQueue;

		MPSCRingBuffer<RenderSubmissionRequest, c_MaxPendingRequests> m_SubmissionQueue;
		MPSCRingBuffer<RenderPresentRequest, c_MaxPendingRequests> m_PresentQueue;
		MPSCRingBuffer<RenderNotification, c_MaxPendingNotifications> m_NotificationQueue;
		MPSCRingBuffer<RenderAcquireRequest, c_MaxPendingAcquires> m_AcquireQueue;

		// This thread is the only ever producer, so these are plain SPSC queues.
		SPSCRingBuffer<RenderFrameCompletion, c_MaxPendingCompletions> m_CompletionQueue;
		SPSCRingBuffer<RenderAcquireResult, c_MaxPendingAcquires> m_AcquireResultQueue;

		// Only ever written by this thread (in Present() issue order, so plain monotonic
		// stores are fine) and read from the main thread via GetLastPresentedFrame(). Sentinel-
		// initialized (not 0) since 0 is a legitimate real frame number - see Render()'s pacing
		// wait, which would otherwise treat "frame 0 has presented" and "nothing has presented
		// yet" as the same value.
		Atomic<uint64> m_LastPresentedFrame{ ~uint64(0) };

		Thread m_Thread;
		Mutex m_Mutex;
		ConditionVariable m_CV;
		Atomic<bool> m_Stop{ false };
	};

}