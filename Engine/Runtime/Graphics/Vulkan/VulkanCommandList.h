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

	private:
		friend class CommandList;
		void BindIndexBufferInternal(BufferHandle buffer, size_t offset);

		DeviceInternal& m_Device;
		VkCommandBuffer m_CommandBuffer;
		VkCommandPool m_CommandPool;
	};
}
