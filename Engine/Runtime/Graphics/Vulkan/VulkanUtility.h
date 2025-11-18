#pragma once

#include "RenderAPI/RenderAPITypes.h"

#include "VulkanCommon.h"

#include "VulkanExtensions.h"

namespace tyr
{
	class VulkanUtility
	{
	public:
		static VkFormat ToVulkanPixelFormat(PixelFormat format);
		static VkColorSpaceKHR ToVulkanColorSpace(ColorSpace format);
		static VkImageLayout ToVulkanImageLayout(ImageLayout layout);
		static VkPrimitiveTopology ToVulkanPrimitiveTopology(PrimitiveTopology topology);
		static VkCullModeFlagBits ToVulkanCullMode(CullMode mode);
		static VkSampleCountFlagBits ToVulkanSampleCount(SampleCount sampleCount);
		static VkBlendFactor ToVulkanBlendFactor(BlendFactor blendFactor);
		static VkBlendOp ToVulkanBlendOp(BlendOp op);
		static VkCompareOp ToVulkanCompareOp(CompareOp op);
		static VkStencilOp ToVulkanStencilOp(StencilOp op);
		static VkPipelineBindPoint ToVulkanPipelineBindPoint(PipelineType pipelineType);
		static VkAttachmentLoadOp ToVulkanAttachmentLoadOp(AttachmentLoadOp op);
		static VkAttachmentStoreOp ToVulkanAttachmentStoreOp(AttachmentStoreOp op);
		static VkDynamicState ToVulkanDynamicState(DynamicState dynamicState);
		static VkDescriptorType ToVulkanDescriptorType(DescriptorType descriptorType);
		static VkVertexInputRate ToVulkanVertexInputRate(VertexInputRate inputRate);
		static VkPolygonMode ToVulkanPolygonMode(PolygonMode mode);
		static VkFrontFace ToVulkanFrontFace(FrontFace face);
		static VkSharingMode ToVulkanSharingMode(SharingMode mode);
		static VkCommandBufferLevel ToVulkanCommandBufferLevel(CommandListType type);
		static VkFilter ToVulkanFilter(Filter filter);
		static VkSamplerMipmapMode ToVulkanSamplerMipmapMode(SamplerMipmapMode mode);
		static VkSamplerAddressMode ToVulkanSamplerAddressMode(SamplerAddressMode mode);
		static VkBorderColor ToVulkanBorderColour(BorderColour colour);

		static inline void SetDebugName(VkDevice device, const char* name, VkObjectType objectType, uint64 objectHandle)
		{
#if !TYR_FINAL
			VkDebugUtilsObjectNameInfoEXT info = {};
			info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
			info.pNext = nullptr;
			info.objectType = objectType;
			info.objectHandle = objectHandle;
			info.pObjectName = name;

			TYR_GASSERT(vkSetDebugUtilsObjectNameEXT(device, &info));
#else
			return;
#endif 
		}
	};

#if TYR_FINAL
#	define TYR_SET_GFX_DEBUG_NAME()
#else
#	define TYR_SET_GFX_DEBUG_NAME(device, name, objectType, objectHandle) VulkanUtility::SetDebugName(device, name, objectType, objectHandle);
#endif
}




