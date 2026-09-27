#include "VulkanSwapChain.h"
#include "VulkanDevice.h"
#include "VulkanCommandQueue.h"
#include "VulkanHelper.h"

namespace tyr
{
	VulkanSwapChain::VulkanSwapChain(void* windowOSHandle, DeviceInternal* device, const SwapChainDesc& desc)
		: SwapChain(device)
		, m_Instance(device->GetInstance())
		, m_LogicalDevice(device->GetLogicalDevice())
	{
		Create(windowOSHandle, desc);
	}

	VulkanSwapChain::~VulkanSwapChain()
	{
		Destroy();
	}

	void VulkanSwapChain::Create(void* windowOSHandle, const SwapChainDesc& desc)
	{
		m_Desc = desc;
		CreateSurface(windowOSHandle);
		CreateSyncData();
		CreateSwapChainAndResources(m_SwapChain, m_ImageData);
	}

	void VulkanSwapChain::Destroy()
	{
		DestroySwapChainAndResources();
		DestroySurface();
	}

	void VulkanSwapChain::DestroySwapChainAndResources()
	{
		if (m_OldSwapChain)
		{
			DestroyOldSwapChain();
		}
		DeleteSwapChainImagesAndViews(m_ImageData);
		vkDestroySwapchainKHR(m_LogicalDevice, m_SwapChain, g_VulkanAllocationCallbacks);
	}

	void VulkanSwapChain::Recreate(void* windowOSHandle, const SwapChainDesc& desc)
	{
		Destroy();
		Create(windowOSHandle, desc);
	}

	void VulkanSwapChain::Resize()
	{
		// A minimized window (or one mid-destruction) reports a 0x0 surface extent - Vulkan
		// allows creating a 0x0 swapchain, but creating image views for a 0x0 image is invalid
		// and crashes some drivers outright. Leave the existing swap chain untouched in that
		// case; if the window comes back to a real size later, another resize notification will
		// naturally follow and this will just work then.
		const DeviceInternal* vulkanDevice = static_cast<DeviceInternal*>(m_Device);
		VkSurfaceCapabilitiesKHR capabilities;
		TYR_GASSERT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vulkanDevice->GetPhysicalDevice(), m_Surface, &capabilities));
		if (capabilities.currentExtent.width == 0 || capabilities.currentExtent.height == 0)
		{
			return;
		}

		TYR_ASSERT(!m_OldSwapChain);
		m_OldImageData = m_ImageData;
		// CreateSwapChainAndResources below fills m_ImageData in via LocalArray::Add() for the
		// new swap chain's images - without clearing it first, those calls would append on top
		// of the old (now-snapshotted-into-m_OldImageData) entries and overflow its fixed
		// capacity (c_MaxImages), corrupting memory.
		m_ImageData = {};
		m_OldSwapChain = m_SwapChain;
		VkSwapchainKHR tempSwapChain{};
		CreateSwapChainAndResources(tempSwapChain, m_ImageData, m_OldSwapChain);
		m_SwapChain = tempSwapChain;
	}

	void VulkanSwapChain::CreateSurface(void* windowOSHandle)
	{
		VulkanHelper::CreateWindowSurface(windowOSHandle, m_Instance, &m_Surface);
	}

	void VulkanSwapChain::DestroySurface()
	{
		vkDestroySurfaceKHR(m_Instance, m_Surface, g_VulkanAllocationCallbacks);
	}

	void VulkanSwapChain::CreateSwapChainAndResources(VkSwapchainKHR& swapChain, SwapChainImageData& imageData, VkSwapchainKHR oldSwapChain)
	{
		const DeviceInternal* vulkanDevice = static_cast<DeviceInternal*>(m_Device);
		VkPhysicalDevice physicalDevice = vulkanDevice->GetPhysicalDevice();

		VkSurfaceCapabilitiesKHR capabilities;
		TYR_GASSERT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, m_Surface, &capabilities));

		uint formatCount;
		TYR_GASSERT(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_Surface, &formatCount, nullptr));

		TYR_ASSERT(formatCount != 0);

		VkSurfaceFormatKHR surfaceFormat = {};

		VkSurfaceFormatKHR* surfaceFormats = StackAlloc<VkSurfaceFormatKHR>(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_Surface, &formatCount, surfaceFormats);

		VkFormat colorFormat = VulkanUtility::ToVulkanPixelFormat(m_Desc.pixelFormat);
		VkColorSpaceKHR colorSpace = VulkanUtility::ToVulkanColorSpace(m_Desc.colorSpace);

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
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_Surface, &presentModeCount, nullptr);

		VkPresentModeKHR* presentModes = StackAlloc<VkPresentModeKHR>(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_Surface, &presentModeCount, presentModes);

		// Guaranteed to be available.
		VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

		if (m_Desc.vSyncEnabled)
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
		imageData.width = capabilities.currentExtent.width;
		imageData.height = capabilities.currentExtent.height;

		// Dimensions of extent is in pixels.
		VkExtent2D extent;
		extent.width = imageData.width;
		extent.height = imageData.height;

		const uint minImageCount = m_Desc.minImageCount;
		TYR_ASSERT(minImageCount >= 2 && minImageCount <= SwapChain::c_MaxImages);

		// Max of 0 means no max.
		if (capabilities.maxImageCount > 0 && capabilities.maxImageCount < minImageCount)
		{
			TYR_ASSERT(false);
			TYR_LOG_FATAL("Vulkan Error: Maximum swap chain image count supported is less than required!");
		}

		VkSwapchainCreateInfoKHR swapChainCI{};
		swapChainCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapChainCI.pNext = nullptr;
		swapChainCI.surface = m_Surface;
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
		swapChainCI.oldSwapchain = oldSwapChain;
		swapChainCI.clipped = VK_TRUE;
		swapChainCI.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

		TYR_GASSERT(vkCreateSwapchainKHR(m_LogicalDevice, &swapChainCI, g_VulkanAllocationCallbacks, &swapChain));

		uint imageCount;
		TYR_GASSERT(vkGetSwapchainImagesKHR(m_LogicalDevice, swapChain, &imageCount, nullptr));

		LocalArray<VkImage, c_MaxImages> images;
		images.Resize(imageCount);

		TYR_GASSERT(vkGetSwapchainImagesKHR(m_LogicalDevice, swapChain, &imageCount, images.Data()));

		CreateSwapChainImagesAndViews(imageData, reinterpret_cast<void**>(images.Data()), imageCount);
	}

	void VulkanSwapChain::DestroyOldSwapChain()
	{
		DeleteOtherSwapChainImagesAndViews();
		vkDestroySwapchainKHR(m_LogicalDevice, m_OldSwapChain, g_VulkanAllocationCallbacks);
		m_OldSwapChain = nullptr;
	}

	uint VulkanSwapChain::AcquireNextImage(SemaphoreHandle semaphore, bool& valid, bool& resizeNeeded)
	{
		// Sentinel, not left uninitialized - the Vulkan spec doesn't guarantee vkAcquireNextImageKHR
		// writes its out-parameter when it fails with VK_ERROR_OUT_OF_DATE_KHR, and this used to
		// return that uninitialized value straight to the caller, which then used it as an array
		// index.
		uint index = UINT_MAX;
		valid = false;
		resizeNeeded = false;

		const DeviceInternal* vulkanDevice = static_cast<DeviceInternal*>(m_Device);
		const Semaphore& semaphoreData = vulkanDevice->GetSemaphore(semaphore);
		VkResult result = vkAcquireNextImageKHR(m_LogicalDevice, m_SwapChain, UINT64_MAX, semaphoreData.semaphore, VK_NULL_HANDLE, &index);
		if (result == VK_SUCCESS)
		{
			valid = true;
		}
		else if (result == VK_SUBOPTIMAL_KHR)
		{
			// Still a real, usable image - just a hint that the swap chain should be recreated.
			valid = true;
			resizeNeeded = true;
		}
		else if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			// No image was acquired - index is left at its sentinel above.
			resizeNeeded = true;
		}
		else
		{
			TYR_ASSERT(false);
			TYR_LOG_FATAL("Error: Vulkan: Failed to acquire next image from the swap chain %d.");
		}
		return index;
	}
	
	void VulkanSwapChain::Present(const CommandQueue* queue, SemaphoreHandle semaphore, uint imageIndex, bool& resizeNeeded)
	{
		resizeNeeded = false;

		const DeviceInternal* vulkanDevice = static_cast<DeviceInternal*>(m_Device);
		const Semaphore& semaphoreData = vulkanDevice->GetSemaphore(semaphore);
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.pNext = nullptr;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &semaphoreData.semaphore;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &m_SwapChain;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr;

		const CommandQueueInternal* queueInternal = static_cast<const CommandQueueInternal*>(queue);
		VkResult result = vkQueuePresentKHR(queueInternal->GetQueue(), &presentInfo);
		if (result != VK_SUCCESS)
		{
			if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
			{
				resizeNeeded = true;
			}
			else
			{
				TYR_ASSERT(false);
				TYR_LOG_FATAL("Error: Vulkan: Failed to present the swap chain's image.");
			}
		}
	}
}