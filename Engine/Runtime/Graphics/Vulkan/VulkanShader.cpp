

#include "VulkanShader.h"
#include "VulkanDevice.h"

namespace tyr
{
	VulkanShader::VulkanShader(VulkanDevice& device, const ShaderDesc& shaderDesc)
		: Shader(shaderDesc)
		, m_Device(device)
		, m_ShaderModule(VK_NULL_HANDLE)
	{
		CreateByteCode();
		Construct();
	}
	
	VulkanShader::~VulkanShader()
	{
		vkDestroyShaderModule(m_Device.GetLogicalDevice(), m_ShaderModule, g_VulkanAllocationCallbacks);
	}

	void VulkanShader::Construct()
	{
		TYR_ASSERT(!m_ByteCode.IsEmpty());

		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.pNext = nullptr;
		createInfo.flags = 0;
		createInfo.codeSize = m_ByteCodeSize;
		createInfo.pCode = reinterpret_cast<const uint32_t*>(m_ByteCode.Data());

		TYR_GASSERT(vkCreateShaderModule(m_Device.GetLogicalDevice(), &createInfo, g_VulkanAllocationCallbacks, &m_ShaderModule));
		VulkanUtility::SetDebugName(m_Device.GetLogicalDevice(), m_Desc.debugName.c_str(), VK_OBJECT_TYPE_SHADER_MODULE, reinterpret_cast<uint64>(m_ShaderModule));
	}
}