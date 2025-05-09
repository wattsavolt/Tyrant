
#pragma once

#include <vulkan/vulkan.h>
#undef None // Conflicting define from Xlib
#include <vma/vk_mem_alloc.h>
#include "Base/Base.h"
#include "String/StringTypes.h"
#include "Containers/Containers.h"
#include "Utility/Utility.h"

#define VULKAN_ERROR(r) r != VK_SUCCESS 

namespace tyr
{
    extern const uint c_VulkanAPIVersion;
    extern const VkAllocationCallbacks* g_VulkanAllocationCallbacks;
}




