#include "VulkanCommandQueue.h"
#include "VulkanCommandList.h"
#include "Memory/StackAllocation.h"
#include "VulkanCommandAllocator.h"
#include "VulkanSync.h"

namespace tyr
{
	CommandQueueInternal::CommandQueueInternal(DeviceInternal& device, const GDebugString& debugName, CommandQueueType queueType, uint queueIndex)
		: CommandQueue(debugName, queueType)
		, m_Device(device)
	{
		m_QueueFamilyIndex = device.GetQueueFamilyIndex(queueType);

		m_Device.GetQueue(queueType, queueIndex);
		
#if !TYR_FINAL
		VulkanUtility::SetDebugName(device.GetLogicalDevice(), debugName.CStr(), VK_OBJECT_TYPE_QUEUE, reinterpret_cast<uint64>(m_Queue));
#endif
	}

	CommandQueueInternal::~CommandQueueInternal()
	{
		
	}

	void CommandQueue::Execute(const CommandQueueExecuteArgs* executeDescs, uint executeDescCount, uint queueIndex, FenceHandle fence)
	{
		TYR_ASSERT(executeDescs && executeDescCount > 0);

		CommandQueueInternal& commandQueue = static_cast<CommandQueueInternal&>(*this);

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

			VkSemaphore* signalSemaphores = nullptr;
			if (executeDesc.signalSemaphoreCount > 0)
			{
				signalSemaphores = stack.Alloc<VkSemaphore>(executeDesc.signalSemaphoreCount);
				for (size_t j = 0; j < executeDesc.signalSemaphoreCount; ++j)
				{
					const Semaphore& semaphore = commandQueue.m_Device.GetSemaphore(executeDesc.signalSemaphores[j]);
					signalSemaphores[j] = semaphore.semaphore;
				}
			}

			VkTimelineSemaphoreSubmitInfo* timelineSemaphoreSubmitInfo = nullptr;
			if (executeDesc.waitValueCount > 0 || executeDesc.signalValueCount > 0)
			{
				timelineSemaphoreSubmitInfo = stack.Alloc<VkTimelineSemaphoreSubmitInfo>();
				timelineSemaphoreSubmitInfo->sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
				timelineSemaphoreSubmitInfo->pNext = nullptr;
				timelineSemaphoreSubmitInfo->waitSemaphoreValueCount = executeDesc.waitValueCount;
				timelineSemaphoreSubmitInfo->pWaitSemaphoreValues = executeDesc.waitValues;
				timelineSemaphoreSubmitInfo->signalSemaphoreValueCount = executeDesc.signalValueCount;
				timelineSemaphoreSubmitInfo->pSignalSemaphoreValues = executeDesc.signalValues;
			}

			submitInfos[i].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfos[i].pNext = timelineSemaphoreSubmitInfo;
			submitInfos[i].waitSemaphoreCount = executeDesc.waitSemaphoreCount;
			submitInfos[i].pWaitSemaphores = waitSemaphores;
			submitInfos[i].pWaitDstStageMask = waitDstPipelineStages;
			submitInfos[i].commandBufferCount = executeDesc.commandListCount;
			submitInfos[i].pCommandBuffers = commandBuffers;
			submitInfos[i].signalSemaphoreCount = executeDesc.signalSemaphoreCount;
			submitInfos[i].pSignalSemaphores = signalSemaphores;
		}

		VkFence vkFence = fence ? commandQueue.m_Device.GetFence(fence).fence : VK_NULL_HANDLE;
		TYR_GASSERT(vkQueueSubmit(commandQueue.m_Queue, executeDescCount, submitInfos, vkFence));
	}
}