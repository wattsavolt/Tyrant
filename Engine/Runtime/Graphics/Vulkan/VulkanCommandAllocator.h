/** Copyright (c) 2019-2021 Aidan Clear */
#pragma once

#include "RenderAPI/CommandAllocator.h"
#include "VulkanDevice.h"

namespace tyr
{
	class VulkanCommandAllocator : public CommandAllocator
	{
	public:
		VulkanCommandAllocator(VulkanDevice& device, const CommandAllocatorDesc& desc);
		~VulkanCommandAllocator();
		void Reset() override;

		VkCommandPool GetCommandPool() const { return m_CommandPool; };
	private:
		VulkanDevice& m_Device;
		VkCommandPool m_CommandPool;
	};
}
