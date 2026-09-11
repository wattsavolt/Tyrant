#pragma once

#include "RenderAPI/CommandList.h"
#include "VulkanDevice.h"

namespace tyr
{
	class Buffer;
	class RenderingInfo;
	class CommandListInternal final : public CommandList
	{
	public:
		CommandListInternal(DeviceInternal& device, const CommandListDesc& desc);
		~CommandListInternal();

		uint GetQueueFamilyIndex() const { return m_QueueFamilyIndex; }

		VkCommandBuffer GetCommandBuffer() const { return m_CommandBuffer; }

	private:
		friend class CommandList;
		void BindIndexBufferInternal(BufferHandle buffer, size_t offset);

		DeviceInternal& m_Device;
		VkCommandBuffer m_CommandBuffer;
		VkCommandPool m_CommandPool;
		uint m_QueueFamilyIndex;
	};
}
