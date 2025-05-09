
#pragma once

#include "Base/Base.h"
#include <vulkan/vulkan.h>

#define TYR_VK_DECLARE_PROC_NULL(fn) inline PFN_##fn fn = nullptr
#define TYR_VK_DECLARE_PROC_LOAD(fn) inline PFN_##fn fn = ::fn
#define TYR_VK_GET_INSTANCE_PROC_ADDR(fn)\
if(!fn)\
{\
	fn = (PFN_##fn)vkGetInstanceProcAddr(instance, #fn);\
	TYR_ASSERT(fn); \
}
#define TYR_VK_GET_DEVICE_PROC_ADDR(fn)\
if (!fn)\
{\
	fn = (PFN_##fn)vkGetDeviceProcAddr(device, #fn);\
	TYR_ASSERT(fn); \
}

namespace tyr
{
	TYR_VK_DECLARE_PROC_NULL(vkSetDebugUtilsObjectNameEXT);
	TYR_VK_DECLARE_PROC_NULL(vkSetDebugUtilsObjectTagEXT);
	TYR_VK_DECLARE_PROC_NULL(vkQueueBeginDebugUtilsLabelEXT);
	TYR_VK_DECLARE_PROC_NULL(vkQueueEndDebugUtilsLabelEXT);
	TYR_VK_DECLARE_PROC_NULL(vkQueueInsertDebugUtilsLabelEXT);
	TYR_VK_DECLARE_PROC_NULL(vkCmdBeginDebugUtilsLabelEXT);
	TYR_VK_DECLARE_PROC_NULL(vkCmdEndDebugUtilsLabelEXT);
	TYR_VK_DECLARE_PROC_NULL(vkCmdInsertDebugUtilsLabelEXT);
	TYR_VK_DECLARE_PROC_NULL(vkCreateDebugUtilsMessengerEXT);
	TYR_VK_DECLARE_PROC_NULL(vkDestroyDebugUtilsMessengerEXT);
	TYR_VK_DECLARE_PROC_NULL(vkSubmitDebugUtilsMessageEXT);
	TYR_VK_DECLARE_PROC_NULL(vkCmdDrawMeshTasksEXT);
}




