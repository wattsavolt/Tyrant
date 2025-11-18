#pragma once

#include "RenderAPI/ShaderModule.h"
#include "VulkanCommon.h"

namespace tyr
{
	// Note: When implementing this in D3D12, this will require a custom pool becauswe shader byte code will need to be allocated, cached and recycled.
	// A copy is only made of the byte code on pipeline creation in D3D12 whereas with Vulkan, the byte code is copied to VRAM on creation of the shader module.
	struct ShaderModule
	{
		LocalString<ShaderModuleDesc::c_MaxEntryPointSize> entryPoint;
		VkShaderModule shaderModule;
		VkShaderStageFlagBits stage;
	};
}
