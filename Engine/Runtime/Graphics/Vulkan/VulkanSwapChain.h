

#pragma once

#include "VulkanDevice.h"
#include "RenderAPI/SwapChain.h"

namespace tyr
{
	class VulkanSwapChain : public SwapChain
	{
	public:
		VulkanSwapChain(VulkanDevice& device, VkSurfaceKHR surface, const SwapChainDesc& desc, VulkanSwapChain* oldSwapChain = nullptr);
		~VulkanSwapChain();

		uint AcquireNextImage(Ref<Semaphore>& semaphore) override;

		void Present(const Ref<CommandList>& commandList, const Ref<Semaphore>& semaphore, uint imageIndex, uint queueIndex = 0u) override;

		VkDevice GetLogicalDevice() const { return m_LogicalDevice; }

		VkSwapchainKHR GetHandle() const { return m_SwapChain; }

	private:
		VkDevice m_LogicalDevice = VK_NULL_HANDLE;
		VkSwapchainKHR m_SwapChain = VK_NULL_HANDLE;
		VkFormat m_ImageFormat;
		Array<VkImage> m_Images;
	};
}
