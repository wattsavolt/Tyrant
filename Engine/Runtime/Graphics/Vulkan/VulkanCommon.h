#pragma once

#include <vulkan/vulkan.h>
#if TYR_PLATFORM == TYR_PLATFORM_LINUX
#undef None // Conflicting define from Xlib
#endif
#include <vma/vk_mem_alloc.h>
#include "Base/Primitives.h"

#define TYR_VULKAN_ERROR(r) r != VK_SUCCESS 

namespace tyr
{
    extern const uint c_VulkanAPIVersion;
    extern const VkAllocationCallbacks* g_VulkanAllocationCallbacks;
}




