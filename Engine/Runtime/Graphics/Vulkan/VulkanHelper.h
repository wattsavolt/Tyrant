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
		// Adds the instance extension if the loader has it. Returns whether it was added.
		static bool AddOptionalInstanceExtension(Array<const char*>& extensions, const char* name);
		static void LoadInstanceExtensionFunctions(VkInstance instance);
		static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* createInfo, VkDebugUtilsMessengerEXT* debugMessenger);
		static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger);
		static void CreateWindowSurface(void* windowOSHandle, VkInstance instance, VkSurfaceKHR* surface);
	};
}
