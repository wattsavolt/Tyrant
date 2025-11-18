#pragma once

#include "RenderAPI/RenderAPI.h"

#include "VulkanCommon.h"
#include "VulkanDevice.h"

namespace tyr
{
	struct VulkanPhyscialDeviceData
	{
		VkPhysicalDevice device;
		uint index;
	};

	class DeviceInternal;
	class VulkanRenderAPI final : public RenderAPI
	{
	public:
		VulkanRenderAPI();
		~VulkanRenderAPI();

	protected:
		void InitializeAPI() override;
		void ShutdownAPI() override;

	private:
		struct QueueFamilyIndices 
		{
			int graphicsFamily = -1;
			int presentFamily = -1;

			bool IsComplete()
			{
				return graphicsFamily > -1 && presentFamily > -1;
			}
		};

		void CreateInstance();
		void SetupDebugMessenger();
		void CreateSurface();
		void SelectPhysicalDevice();
		int RatePhysicalDevice(VkPhysicalDevice device);
		QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
		bool DeviceExtensionsSupported(VkPhysicalDevice device);

		static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
			VkDebugUtilsMessageTypeFlagsEXT messageType,
			const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
			void* userData);

		VkInstance m_Instance = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
		VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
		// Implicitly destroyed automatically when instance is destroyed.
		VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
		uint m_PhysicalDeviceIndex = UINT32_MAX;

		static const Array<const char*> c_ValidationLayers;
	};
}
