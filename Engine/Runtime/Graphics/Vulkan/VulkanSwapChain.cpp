#include "VulkanSwapChain.h"
#include "VulkanDevice.h"
#include "VulkanCommandList.h"

namespace tyr
{
	VulkanSwapChain::VulkanSwapChain(DeviceInternal& device, VkSurfaceKHR surface, const SwapChainDesc& desc, VulkanSwapChain* oldSwapChain)
		: SwapChain(device, desc)
	{
		auto physicalDevice = device.GetPhysicalDevice();
		m_LogicalDevice = device.GetLogicalDevice();

		VkSurfaceCapabilitiesKHR capabilities;
		TYR_GASSERT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities));
		
		uint formatCount;
		TYR_GASSERT(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr));

		TYR_ASSERT(formatCount != 0);

		VkSurfaceFormatKHR surfaceFormat = {};

		VkSurfaceFormatKHR* surfaceFormats = StackAlloc<VkSurfaceFormatKHR>(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, surfaceFormats);
		
		VkFormat colorFormat = VulkanUtility::ToVulkanPixelFormat(desc.pixelFormat);
		VkColorSpaceKHR colorSpace = VulkanUtility::ToVulkanColorSpace(desc.colorSpace);

		bool formatFound = false;
		for (uint i = 0; i < formatCount; ++i)
		{
			const auto& sf = surfaceFormats[i];
			if (sf.format == colorFormat && sf.colorSpace == colorSpace) 
			{
				formatFound = true;
				surfaceFormat = sf;
				break;
			}
		}

		TYR_ASSERT(formatFound);

		StackFreeLast();

		uint presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);

		VkPresentModeKHR* presentModes = StackAlloc<VkPresentModeKHR>(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes);

		// Guaranteed to be available.
		VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

		if (desc.vSyncEnabled)
		{
			// Maybe don't use this on phones. Mailbox results in lower input latency than FIFO but it can waste GPU power 
			// by rendering frames that are never displayed, especially if the app runs much faster than the refresh rate. 
			for (uint i = 0; i < presentModeCount; ++i)
			{
				VkPresentModeKHR pm = presentModes[i];
				if (pm == VK_PRESENT_MODE_MAILBOX_KHR)
				{
					presentMode = pm;
					break;
				}
			}
		}
		else
		{
			for (uint i = 0; i < presentModeCount; ++i)
			{
				VkPresentModeKHR pm = presentModes[i];
				if (pm == VK_PRESENT_MODE_IMMEDIATE_KHR)
				{
					presentMode = pm;
					break;
				}

				if (presentModes[i] == VK_PRESENT_MODE_FIFO_RELAXED_KHR)
				{
					presentMode = pm;
				}
			}
		}

		StackFreeLast();

		// If resolution of window is not supported, then width in capabilities will be UINT32_MAX
		TYR_ASSERT(capabilities.currentExtent.width != UINT32_MAX);
		m_Width = capabilities.currentExtent.width;
		m_Height = capabilities.currentExtent.height;

		// Dimensions of extent is in pixels.
		VkExtent2D extent;
		extent.width = m_Width;
		extent.height = m_Height;

		const uint minImageCount = desc.useTripleBuffering ? 3 : 2;

		// Max of 0 means no max.
		if (capabilities.maxImageCount > 0 && capabilities.maxImageCount < minImageCount)
		{
			TYR_ASSERT(false);
			TYR_LOG_FATAL("Vulkan Error: Maximum swap chain image count supported is less than required!");
		}

		VkSwapchainCreateInfoKHR swapChainCI{};
		swapChainCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapChainCI.pNext = nullptr;
		swapChainCI.surface = surface;
		swapChainCI.minImageCount = minImageCount;
		swapChainCI.imageFormat = surfaceFormat.format;
		swapChainCI.imageColorSpace = surfaceFormat.colorSpace;
		swapChainCI.imageExtent = extent;
		swapChainCI.imageArrayLayers = 1; // More than 1 only for stereoscopic (VR) apps.
		// Note: Use VK_IMAGE_USAGE_TRANSFER_DST_BIT if rendering to another image first for post-processing.
		swapChainCI.imageUsage = static_cast<VkImageUsageFlags>(m_ImageUsage);
		swapChainCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // Present and graphics queue should be the same.
		swapChainCI.queueFamilyIndexCount = 0;
		swapChainCI.pQueueFamilyIndices = nullptr;
		swapChainCI.preTransform = capabilities.currentTransform;
		swapChainCI.presentMode = presentMode;
		swapChainCI.oldSwapchain = oldSwapChain ? oldSwapChain->m_SwapChain : VK_NULL_HANDLE;
		swapChainCI.clipped = VK_TRUE;
		swapChainCI.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		
		TYR_GASSERT(vkCreateSwapchainKHR(m_LogicalDevice, &swapChainCI, g_VulkanAllocationCallbacks, &m_SwapChain));
		
		uint imageCount;
		TYR_GASSERT(vkGetSwapchainImagesKHR(m_LogicalDevice, m_SwapChain, &imageCount, nullptr));

		m_Images.Resize(imageCount);
		TYR_GASSERT(vkGetSwapchainImagesKHR(m_LogicalDevice, m_SwapChain, &imageCount, m_Images.Data()));

		m_ImageFormat = surfaceFormat.format;	

		CreateSwapChainImagesAndViews(reinterpret_cast<Handle*>(m_Images.Data()), imageCount);

		// TODO: Create framebuffers from these images if using render passes.
	}

	VulkanSwapChain::~VulkanSwapChain()
	{
		DeleteSwapChainImagesAndViews();
		vkDestroySwapchainKHR(m_LogicalDevice, m_SwapChain, g_VulkanAllocationCallbacks);
	}

	uint VulkanSwapChain::AcquireNextImage(SemaphoreHandle semaphore) 
	{
		uint index;
		const DeviceInternal& vulkanDevice = static_cast<DeviceInternal&>(m_Device);
		const Semaphore& semaphoreData = vulkanDevice.GetSemaphore(semaphore);
		VkResult result = vkAcquireNextImageKHR(m_LogicalDevice, m_SwapChain, UINT64_MAX, semaphoreData.semaphore, VK_NULL_HANDLE, &index);
		if (result != VK_SUCCESS)
		{
			if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
			{
				m_Resized = true;
			}
			else
			{
				TYR_ASSERT(false);
				TYR_LOG_FATAL("Error: Vulkan: Failed to acquire next image from the swap chain %d.");
			}
		}
		return index;
	}
	
	void VulkanSwapChain::Present(const CommandList* commandList, SemaphoreHandle semaphore, uint imageIndex, uint queueIndex)
	{
		const DeviceInternal& vulkanDevice = static_cast<DeviceInternal&>(m_Device);
		const Semaphore& semaphoreData = vulkanDevice.GetSemaphore(semaphore);
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.pNext = nullptr;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &semaphoreData.semaphore;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &m_SwapChain;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr;

		VkQueue queue = vulkanDevice.GetQueue(commandList->GetQueueType(), queueIndex);
		VkResult result = vkQueuePresentKHR(queue, &presentInfo);
		if (result != VK_SUCCESS)
		{
			if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
			{
				m_Resized = true;
			}
			else
			{
				TYR_ASSERT(false);
				TYR_LOG_FATAL("Error: Vulkan: Failed to present the swap chain's image.");
			}
		}
	}
}