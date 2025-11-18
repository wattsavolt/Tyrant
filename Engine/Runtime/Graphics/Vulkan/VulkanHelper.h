#pragma once

#include "VulkanCommon.h"
#include "Containers/Array.h"

namespace tyr
{
	// Helper class for VulkanRenderAPI
	class VulkanHelper final
	{
	public:
		static bool ValidationLayersSupported(const Array<const char*>& validationLayers);
		static void DisplayAvailableExtensions();
		static void GetRequiredInstanceExtensions(Array<const char*>& extensions, bool validationLayersEnabled);
		static void LoadInstanceExtensionFunctions(VkInstance instance);
		static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* createInfo, VkDebugUtilsMessengerEXT* debugMessenger);
		static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger);
		static void CreateWindowSurface(void* windowHandle, VkInstance instance, VkSurfaceKHR* surface);
	};
}
