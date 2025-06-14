#include "VulkanRenderAPI.h"
#include "VulkanHelper.h"
#include "VulkanDevice.h"
#include "VulkanSwapChain.h"
#include "VulkanImage.h"

namespace tyr
{
	static const uint c_VulkanAPIVersion = VK_API_VERSION_1_4;
	const VkAllocationCallbacks* g_VulkanAllocationCallbacks = nullptr;

	const Array<const char*> VulkanRenderAPI::c_ValidationLayers =
	{
		"VK_LAYER_KHRONOS_validation"
	};

	VulkanRenderAPI::VulkanRenderAPI()
	{

	}

	VulkanRenderAPI::~VulkanRenderAPI()
	{

	}

	void VulkanRenderAPI::InitializeAPI()
	{
		CreateInstance();
		SetupDebugMessenger();
		CreateSurface();
		SelectPhysicalDevice();
		
		m_Device = new VulkanDevice(m_Instance, m_PhysicalDevice, m_PhysicalDeviceIndex);

		// This may be created by the API user later.
		SwapChainDesc swapChainDesc;
		swapChainDesc.pixelFormat = m_Config.outputPixelFormat;
		swapChainDesc.colorSpace = m_Config.outputColorSpace;
		// TODO: Enable later
		swapChainDesc.createDepth = false;
		swapChainDesc.vSyncEnabled = m_Config.vSyncEnabled;
		swapChainDesc.useTripleBuffering = m_Config.useTripleBuffering;

		m_SwapChain = new VulkanSwapChain(*static_cast<VulkanDevice*>(m_Device), m_Surface, swapChainDesc);
	}

	void VulkanRenderAPI::ShutdownAPI()
	{
		TYR_SAFE_DELETE(m_SwapChain);
		TYR_SAFE_DELETE(m_Device);

		if (m_ValidationLayersEnabled) 
		{
			VulkanHelper::DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger);
		}

		vkDestroySurfaceKHR(m_Instance, m_Surface, g_VulkanAllocationCallbacks);

		vkDestroyInstance(m_Instance, g_VulkanAllocationCallbacks);
	}

	void VulkanRenderAPI::CreateInstance()
	{
		TYR_ASSERT(VulkanHelper::ValidationLayersSupported(c_ValidationLayers));

#if TYR_DEBUG 
		VulkanHelper::DisplayAvailableExtensions();
#endif

		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pNext = nullptr;
		appInfo.pApplicationName = m_Config.appName.c_str();
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "Tyrant Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = c_VulkanAPIVersion;

		Array<const char*> extensions;
		extensions.Reserve(5);
		VulkanHelper::GetRequiredInstanceExtensions(extensions, m_ValidationLayersEnabled);

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pNext = nullptr;
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledExtensionCount = static_cast<uint>(extensions.Size());
		createInfo.ppEnabledExtensionNames = extensions.Data();
		if (m_ValidationLayersEnabled)
		{
			createInfo.enabledLayerCount = static_cast<uint>(c_ValidationLayers.Size());
			createInfo.ppEnabledLayerNames = c_ValidationLayers.Data();
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}
		
		TYR_GASSERT(vkCreateInstance(&createInfo, g_VulkanAllocationCallbacks, &m_Instance));

#if TYR_DEBUG 
		VulkanHelper::LoadInstanceExtensionFunctions(m_Instance);
#endif
	}

	void VulkanRenderAPI::SetupDebugMessenger()
	{
		if (!m_ValidationLayersEnabled)
		{
			return;
		}

		VkDebugUtilsMessengerCreateInfoEXT createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.pNext = nullptr;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = DebugCallback;
		createInfo.pUserData = nullptr;

		TYR_GASSERT(VulkanHelper::CreateDebugUtilsMessengerEXT(m_Instance, &createInfo, &m_DebugMessenger));
	}

	void VulkanRenderAPI::CreateSurface()
	{
		VulkanHelper::CreateWindowSurface(m_Config.windowHandle, m_Instance, &m_Surface);
	}

	void VulkanRenderAPI::SelectPhysicalDevice()
	{
		uint deviceCount = 0;
		vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);

		if (deviceCount == 0) 
		{
			TYR_ASSERT(false);
			TYR_LOG_FATAL("Failed to find GPUs with Vulkan support!");
		}

		Array<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.Data());

		// Use an ordered map to automatically sort candidates by increasing score
		MultiMap<int, VulkanPhyscialDeviceData> candidates;
		for (uint i = 0; i < devices.Size(); ++i)
		{
			VulkanPhyscialDeviceData data;
			data.device = devices[i];
			data.index = i;
			int score = RatePhysicalDevice(data.device);
			candidates.insert(std::make_pair(score, std::move(data)));
		}

		// Check if the best candidate is suitable 
		if (candidates.rbegin()->first > 0) 
		{
			m_PhysicalDevice = candidates.rbegin()->second.device;
			m_PhysicalDeviceIndex = candidates.rbegin()->second.index;
		}
		else 
		{
			TYR_ASSERT(false);
			TYR_LOG_FATAL("Failed to find a suitable GPU!");
		}
	}

	int VulkanRenderAPI::RatePhysicalDevice(VkPhysicalDevice device)
	{
		VkPhysicalDeviceProperties deviceProperties;
		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		// Application can't function without geometry shaders
		if (!deviceFeatures.geometryShader)
		{
			return 0;
		}

		// All required queue families must be supported
		QueueFamilyIndices indices = FindQueueFamilies(device);
		if (!indices.IsComplete())
		{
			return 0;
		}

		if (!DeviceExtensionsSupported(device))
		{
			return 0;
		}

		int score = 0;

		// Discrete GPUs have a significant performance advantage
		if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) 
		{
			score += 1000;
		}

		// Maximum possible size of images affects graphics quality
		score += deviceProperties.limits.maxImageDimension2D;

		return score;
	}

	VulkanRenderAPI::QueueFamilyIndices VulkanRenderAPI::FindQueueFamilies(VkPhysicalDevice device)
	{
		VulkanRenderAPI::QueueFamilyIndices indices;

		uint queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		Array<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.Data());

		int i = 0;
		for (const auto& queueFamily : queueFamilies) 
		{
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) 
			{
				indices.graphicsFamily = i;
			}

			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &presentSupport);

			if (presentSupport)
			{
				indices.presentFamily = i;
			}

			if (indices.IsComplete()) 
			{
				break;
			}

			i++;
		}

		return indices;
	}

	bool VulkanRenderAPI::DeviceExtensionsSupported(VkPhysicalDevice device) 
	{
		uint extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		Array<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.Data());

		size_t count = VulkanDevice::c_RequiredDeviceExtensions.Size();
		for (const auto& extension : availableExtensions) 
		{
			for (const auto& de : VulkanDevice::c_RequiredDeviceExtensions)
			{
				if (strcmp(extension.extensionName, de) == 0)
				{
					count--;
					break;
				}
			}
		}

		return count == 0;
	}

	VKAPI_ATTR VkBool32 VKAPI_CALL VulkanRenderAPI::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
		void* userData)
	{
		if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) 
		{
			TYR_LOG_WARNING("Vulkan Warning: %s", callbackData->pMessage);
			TYR_ASSERT(false);
		}
		else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		{
			TYR_LOG_ERROR("Vulkan Error: %s", callbackData->pMessage);
			TYR_ASSERT(false);
		}

		// Always return false. Change to true just when testing validation layers themselves.
		return VK_FALSE;
	}
}