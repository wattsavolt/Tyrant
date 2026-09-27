#include "VulkanCommandQueue.h"
#include "VulkanCommandList.h"
#include "Memory/StackAllocation.h"
#include "VulkanCommandAllocator.h"
#include "VulkanSync.h"

namespace tyr
{
	CommandQueueInternal::CommandQueueInternal(DeviceInternal& device, const GDebugString& debugName, CommandQueueType queueType, uint queueIndex)
		: CommandQueue(device, debugName, queueType)
		, m_Device(device)
	{
		m_QueueFamilyIndex = device.GetQueueFamilyIndex(queueType);

		m_Queue = m_Device.GetQueue(queueType, queueIndex);
		
#if !TYR_FINAL
		VulkanUtility::SetDebugName(device.GetLogicalDevice(), debugName.CStr(), VK_OBJECT_TYPE_QUEUE, reinterpret_cast<uint64>(m_Queue));
#endif
	}

	CommandQueueInternal::~CommandQueueInternal()
	{
		
	}

	uint64 CommandQueue::Execute(const CommandQueueExecuteArgs* executeDescs, uint executeDescCount, uint queueIndex, FenceHandle fence)
	{
		TYR_ASSERT(executeDescs && executeDescCount > 0);

		CommandQueueInternal& commandQueue = static_cast<CommandQueueInternal&>(*this);

		const uint64 timelineValue = m_NextTimelineValue.fetch_add(1, std::memory_order_relaxed) + 1;
		const VkSemaphore vkTimelineSemaphore = commandQueue.m_Device.GetSemaphore(m_TimelineSemaphore).semaphore;

		StackAllocManager stack;
		VkSubmitInfo* submitInfos = stack.Alloc<VkSubmitInfo>(executeDescCount);
		for (uint i = 0; i < executeDescCount; ++i)
		{
			const CommandQueueExecuteArgs& executeDesc = executeDescs[i];
			TYR_ASSERT(executeDesc.commandListCount > 0);

			// The command lists should include this one.
			VkCommandBuffer* commandBuffers = stack.Alloc<VkCommandBuffer>(executeDesc.commandListCount);
			for (size_t j = 0; j < executeDesc.commandListCount; ++j)
			{
				const CommandListInternal* cmdList = static_cast<CommandListInternal*>(executeDesc.commandLists[j]);
				commandBuffers[j] = cmdList->GetCommandBuffer();
			}

			VkSemaphore* waitSemaphores = nullptr;
			if (executeDesc.waitSemaphoreCount > 0)
			{
				waitSemaphores = stack.Alloc<VkSemaphore>(executeDesc.waitSemaphoreCount);
				for (size_t j = 0; j < executeDesc.waitSemaphoreCount; ++j)
				{
					const Semaphore& semaphore = commandQueue.m_Device.GetSemaphore(executeDesc.waitSemaphores[j]);
					waitSemaphores[j] = semaphore.semaphore;
				}
			}

			VkPipelineStageFlags* waitDstPipelineStages = nullptr;
			if (executeDesc.waitDstPipelineStageCount > 0)
			{
				waitDstPipelineStages = stack.Alloc<VkPipelineStageFlags>(executeDesc.waitDstPipelineStageCount);
				for (size_t j = 0; j < executeDesc.waitDstPipelineStageCount; ++j)
				{
					waitDstPipelineStages[j] = static_cast<VkPipelineStageFlags>(executeDesc.waitDstPipelineStages[j]);
				}
			}

			// This queue's own timeline semaphore is always added as one more signal, on top
			// of whatever the caller asked for.
			const uint signalSemaphoreCount = executeDesc.signalSemaphoreCount + 1;
			VkSemaphore* signalSemaphores = stack.Alloc<VkSemaphore>(signalSemaphoreCount);
			for (size_t j = 0; j < executeDesc.signalSemaphoreCount; ++j)
			{
				const Semaphore& semaphore = commandQueue.m_Device.GetSemaphore(executeDesc.signalSemaphores[j]);
				signalSemaphores[j] = semaphore.semaphore;
			}
			signalSemaphores[executeDesc.signalSemaphoreCount] = vkTimelineSemaphore;

			uint64* signalValues = stack.Alloc<uint64>(signalSemaphoreCount);
			for (size_t j = 0; j < executeDesc.signalValueCount; ++j)
			{
				signalValues[j] = executeDesc.signalValues[j];
			}
			signalValues[executeDesc.signalSemaphoreCount] = timelineValue;

			VkTimelineSemaphoreSubmitInfo* timelineSemaphoreSubmitInfo = stack.Alloc<VkTimelineSemaphoreSubmitInfo>();
			timelineSemaphoreSubmitInfo->sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
			timelineSemaphoreSubmitInfo->pNext = nullptr;
			timelineSemaphoreSubmitInfo->waitSemaphoreValueCount = executeDesc.waitValueCount;
			timelineSemaphoreSubmitInfo->pWaitSemaphoreValues = executeDesc.waitValues;
			timelineSemaphoreSubmitInfo->signalSemaphoreValueCount = signalSemaphoreCount;
			timelineSemaphoreSubmitInfo->pSignalSemaphoreValues = signalValues;

			submitInfos[i].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfos[i].pNext = timelineSemaphoreSubmitInfo;
			submitInfos[i].waitSemaphoreCount = executeDesc.waitSemaphoreCount;
			submitInfos[i].pWaitSemaphores = waitSemaphores;
			submitInfos[i].pWaitDstStageMask = waitDstPipelineStages;
			submitInfos[i].commandBufferCount = executeDesc.commandListCount;
			submitInfos[i].pCommandBuffers = commandBuffers;
			submitInfos[i].signalSemaphoreCount = signalSemaphoreCount;
			submitInfos[i].pSignalSemaphores = signalSemaphores;
		}

		VkFence vkFence = fence ? commandQueue.m_Device.GetFence(fence).fence : VK_NULL_HANDLE;
		TYR_GASSERT(vkQueueSubmit(commandQueue.m_Queue, executeDescCount, submitInfos, vkFence));

		return timelineValue;
	}
}