#pragma once

#include "RenderAPI/SwapChain.h"
#include "VulkanCommon.h"

namespace tyr
{
	class DeviceInternal;
	class VulkanSwapChain : public SwapChain
	{
	public:
		VulkanSwapChain(DeviceInternal& device, VkSurfaceKHR surface, const SwapChainDesc& desc, VulkanSwapChain* oldSwapChain = nullptr);
		~VulkanSwapChain();

		uint AcquireNextImage(SemaphoreHandle semaphore) override;

		void Present(const CommandList* commandList, SemaphoreHandle semaphore, uint imageIndex, uint queueIndex = 0u) override;

		VkDevice GetLogicalDevice() const { return m_LogicalDevice; }

		VkSwapchainKHR GetHandle() const { return m_SwapChain; }

	private:
		VkDevice m_LogicalDevice = VK_NULL_HANDLE;
		VkSwapchainKHR m_SwapChain = VK_NULL_HANDLE;
		VkFormat m_ImageFormat;
		Array<VkImage> m_Images;
	};
}
