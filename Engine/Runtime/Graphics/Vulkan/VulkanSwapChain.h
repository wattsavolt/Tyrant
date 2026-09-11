#pragma once

#include "RenderAPI/SwapChain.h"
#include "VulkanCommon.h"

namespace tyr
{
	class DeviceInternal;
	class VulkanSwapChain final : public SwapChain
	{
	public:
		VulkanSwapChain(void* windowOSHandle, DeviceInternal* device, const SwapChainDesc& desc);
		~VulkanSwapChain();

		void Create(void* windowOSHandle, const SwapChainDesc& desc);

		void Destroy();

		void DestroySwapChainAndResources();

		void Recreate(void* windowOSHandle, const SwapChainDesc& desc) override;

		void Resize() override;

		void DestroyOldSwapChain() override;

		uint AcquireNextImage(SemaphoreHandle semaphore, bool& resized) override;

		void Present(const CommandQueue* queue, SemaphoreHandle semaphore, uint imageIndex, bool& resized) override;

		VkDevice GetLogicalDevice() const { return m_LogicalDevice; }

		VkSurfaceKHR GetSurface() const { return m_Surface; }

		VkSwapchainKHR GetHandle() const { return m_SwapChain; }

	private:
		void CreateSurface(void* windowOSHandle);
		void DestroySurface();
		void CreateSwapChainAndResources(VkSwapchainKHR& swapChain, SwapChainImageData& imageData, VkSwapchainKHR oldSwapChain = VK_NULL_HANDLE);

		VkInstance m_Instance;
		VkDevice m_LogicalDevice;
		VkSurfaceKHR m_Surface;
		VkSwapchainKHR m_SwapChain = VK_NULL_HANDLE;
		VkSwapchainKHR m_OldSwapChain = VK_NULL_HANDLE;
	};
}
