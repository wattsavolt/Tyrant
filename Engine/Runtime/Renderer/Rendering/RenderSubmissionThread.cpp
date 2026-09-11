#include "RenderSubmissionThread.h"
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
		
	}

	RenderSubmissionThread::~RenderSubmissionThread()
	{
		
	}

	void RenderSubmissionThread::Run()
	{
		// TODO : Implement
	}

	void RenderSubmissionThread::EnqueueRenderSubmissionRequest(const RenderSubmissionRequest& request)
	{
		// TODO : Implement
	}

	void RenderSubmissionThread::EnqueueRenderPresentRequest(const RenderPresentRequest& request)
	{
		// TODO : Implement
	}

	void RenderSubmissionThread::EnqueueRenderNotification(const RenderNotification& notification)
	{
		// TODO : Implement
	}

	void RenderSubmissionThread::ProcessRequests()
	{
		// TODO : Implement
	}
}