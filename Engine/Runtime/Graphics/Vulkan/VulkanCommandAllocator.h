/** Copyright (c) 2019-2021 Aidan Clear */
#pragma once

#include "RenderAPI/CommandAllocator.h"
#include "VulkanCommon.h"

namespace tyr
{
	class DeviceInternal;
	class VulkanCommandAllocator : public CommandAllocator
	{
	public:
		VulkanCommandAllocator(DeviceInternal& device, const CommandAllocatorDesc& desc);
		~VulkanCommandAllocator();
		void Reset() override;

		VkCommandPool GetCommandPool() const { return m_CommandPool; };
	private:
		DeviceInternal& m_Device;
		VkCommandPool m_CommandPool;
	};
}
