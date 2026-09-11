#pragma once

#include "RenderAPI/CommandQueue.h"
#include "VulkanDevice.h"

namespace tyr
{
	class CommandQueueInternal final : public CommandQueue
	{
	public:
		CommandQueueInternal(DeviceInternal& device, const GDebugString& debugName, CommandQueueType queueType, uint queueIndex);
		~CommandQueueInternal();

		uint GetQueueFamilyIndex() const { return m_QueueFamilyIndex; }

		VkQueue GetQueue() const { return m_Queue; }

	private:
		friend class CommandQueue;

		DeviceInternal& m_Device;
		VkQueue m_Queue;
		uint m_QueueFamilyIndex;;
	};
}
