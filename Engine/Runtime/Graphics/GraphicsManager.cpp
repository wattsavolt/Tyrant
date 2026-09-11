#include "GraphicsManager.h"

#if TYR_PLATFORM == TYR_PLATFORM_WINDOWS
#include "Vulkan/VulkanRenderAPI.h"
#endif

namespace tyr
{
	// TODO:: Support more graphics apis.
	RenderAPI* GraphicsManager::CreateRenderAPI(const RenderAPICreateConfig& config)
	{
		RenderAPI* renderAPI;

#if TYR_PLATFORM == TYR_PLATFORM_WINDOWS
		if (config.backend == RenderAPIBackend::Vulkan)
		{
			renderAPI = new VulkanRenderAPI();
		}
#else
		renderAPI = new VulkanRenderAPI();
#endif

		return renderAPI;
	}

	void GraphicsManager::DestroyRenderAPI(RenderAPI* renderAPI)
	{
		delete renderAPI;
	}
}