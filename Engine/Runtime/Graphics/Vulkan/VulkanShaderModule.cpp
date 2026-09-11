#include "VulkanShaderModule.h"
#include "RenderAPI/ShaderModule.h"
#include "VulkanDevice.h"

namespace tyr
{
	ShaderModuleHandle Device::CreateShaderModule(const ShaderModuleDesc& desc)
	{
		TYR_ASSERT(desc.byteCode);

		DeviceInternal& device = static_cast<DeviceInternal&>(*this);		
		const ShaderModuleHandle handle(device.m_ShaderModulePool.Create());
		ShaderModule& shader = device.m_ShaderModulePool[handle.h];

		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.pNext = nullptr;
		createInfo.flags = 0;
		createInfo.codeSize = desc.size;
		createInfo.pCode = reinterpret_cast<const uint32_t*>(desc.byteCode);

		TYR_GASSERT(vkCreateShaderModule(device.m_LogicalDevice, &createInfo, g_VulkanAllocationCallbacks, &shader.shaderModule));
#if !TYR_FINAL
		VulkanUtility::SetDebugName(device.m_LogicalDevice, desc.debugName, VK_OBJECT_TYPE_SHADER_MODULE, reinterpret_cast<uint64>(shader.shaderModule));
#endif
		shader.entryPoint = desc.entryPoint;
		shader.stage = static_cast<VkShaderStageFlagBits>(desc.stage);

		return handle;
	}
	
	void Device::DeleteShaderModule(ShaderModuleHandle handle)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		ShaderModule& shader = device.GetShaderModule(handle);
		vkDestroyShaderModule(device.m_LogicalDevice, shader.shaderModule, g_VulkanAllocationCallbacks);
		device.m_ShaderModulePool.Delete(handle.h);
	}

	ShaderBinaryLanguage Device::GetShaderBinaryLanguage() const { return ShaderBinaryLanguage::SpirV; };
}