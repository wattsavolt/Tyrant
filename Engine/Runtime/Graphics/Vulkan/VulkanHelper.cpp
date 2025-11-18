#include "VulkanHelper.h"
#include "Core.h"
#include "VulkanExtensions.h"

namespace tyr
{
#if TYR_PLATFORM == TYR_PLATFORM_WINDOWS
#define VULKAN_OS_SURFACE_EXTENSION_NAME VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#endif

	bool VulkanHelper::ValidationLayersSupported(const Array<const char*>& validationLayers)
	{
		uint layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : validationLayers) 
		{
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers) 
			{
				if (strcmp(layerName, layerProperties.layerName) == 0) 
				{
					layerFound = true;
					break;
				}
			}

			if (!layerFound) 
			{
				return false;
			}
		}

		return true;
	}

	void VulkanHelper::DisplayAvailableExtensions()
	{
		// Enumerate extensions
		uint extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

		Array<VkExtensionProperties> extensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.Data());

		StringStream ss;
		ss << "Avalilable Vulkan Extensions: " << std::endl;
		for (const auto& ep : extensions)
		{
			ss << ep.extensionName << std::endl;
		}
		const String s = ss.str();
		TYR_LOG_INFO(s.c_str());
	}

	void VulkanHelper::GetRequiredInstanceExtensions(Array<const char*>& extensions, bool validationLayersEnabled)
	{
		extensions.Add(VK_KHR_SURFACE_EXTENSION_NAME);

		if (validationLayersEnabled) 
		{
			extensions.Add(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

#ifdef VULKAN_OS_SURFACE_EXTENSION_NAME
		extensions.Add(VULKAN_OS_SURFACE_EXTENSION_NAME);
#endif
	}

	void VulkanHelper::LoadInstanceExtensionFunctions(VkInstance instance)
	{
		// Debug Utils
		TYR_VK_GET_INSTANCE_PROC_ADDR(vkSetDebugUtilsObjectNameEXT);
		TYR_VK_GET_INSTANCE_PROC_ADDR(vkSetDebugUtilsObjectTagEXT);
		TYR_VK_GET_INSTANCE_PROC_ADDR(vkQueueBeginDebugUtilsLabelEXT);
		TYR_VK_GET_INSTANCE_PROC_ADDR(vkQueueEndDebugUtilsLabelEXT);
		TYR_VK_GET_INSTANCE_PROC_ADDR(vkQueueInsertDebugUtilsLabelEXT);
		TYR_VK_GET_INSTANCE_PROC_ADDR(vkCmdBeginDebugUtilsLabelEXT);
		TYR_VK_GET_INSTANCE_PROC_ADDR(vkCmdEndDebugUtilsLabelEXT);
		TYR_VK_GET_INSTANCE_PROC_ADDR(vkCmdInsertDebugUtilsLabelEXT);
		TYR_VK_GET_INSTANCE_PROC_ADDR(vkCreateDebugUtilsMessengerEXT);
		TYR_VK_GET_INSTANCE_PROC_ADDR(vkDestroyDebugUtilsMessengerEXT);
		TYR_VK_GET_INSTANCE_PROC_ADDR(vkSubmitDebugUtilsMessageEXT);
		TYR_VK_GET_INSTANCE_PROC_ADDR(vkCmdDrawMeshTasksEXT);
	}

	VkResult VulkanHelper::CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* createInfo, VkDebugUtilsMessengerEXT* debugMessenger) 
	{	
		return vkCreateDebugUtilsMessengerEXT(instance, createInfo, g_VulkanAllocationCallbacks, debugMessenger);
	}

	void VulkanHelper::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger)
	{
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr) 
		{
			func(instance, debugMessenger, g_VulkanAllocationCallbacks);
		}
	}

	void VulkanHelper::CreateWindowSurface(void* windowHandle, VkInstance instance, VkSurfaceKHR* surface)
	{
#if TYR_PLATFORM == TYR_PLATFORM_WINDOWS
		VkWin32SurfaceCreateInfoKHR createInfo{};

		// Any static or global function within the dll will do for below.
		// Could also get hInstance of dll from DllMain
		// If we were in exe, we would call GetModuleHandle(NULL) instead.
		/*if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
			GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			(LPCSTR)CreateWindowSurface, &createInfo.hinstance) == 0)
		{
			DWORD e = GetLastError();
			StringStream ss;
			ss << "Failed to load module handle: " << e << ".";
			TYR_LOG_FATAL(ss.str().c_str());
		}*/

		createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		createInfo.pNext = nullptr;
		createInfo.hwnd = static_cast<HWND>(windowHandle);
		createInfo.hinstance = GetModuleHandle(nullptr);

		if (TYR_VULKAN_ERROR(vkCreateWin32SurfaceKHR(instance, &createInfo, g_VulkanAllocationCallbacks, surface)))
		{
			TYR_LOG_FATAL("Failed to create Win32 window surface!");
		}
#endif
		// TODO: Use vkCreateXcbSurfaceKHR for Linux support.
		
	}
}


