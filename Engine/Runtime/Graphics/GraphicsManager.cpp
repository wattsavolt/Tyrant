

#include "GraphicsManager.h"

#if TYR_PLATFORM == TYR_PLATFORM_WINDOWS
#include "Vulkan/VulkanRenderAPI.h"
#endif

namespace tyr
{
	// TODO:: Support more graphics apis.
	Ref<RenderAPI> GraphicsManager::CreateRenderAPI(const RenderAPICreateConfig& config)
	{
		Ref<RenderAPI> api;

#if TYR_PLATFORM == TYR_PLATFORM_WINDOWS
		if (config.backend == RenderAPIBackend::Vulkan)
		{
			api = MakeRef<VulkanRenderAPI>();
		}
#else
		api = MakeRef<VulkanRenderAPI>();
#endif

		return api;
	}
}