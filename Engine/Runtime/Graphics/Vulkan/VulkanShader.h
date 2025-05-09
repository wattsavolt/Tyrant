

#pragma once

#include "RenderAPI/Shader.h"
#include "VulkanCommon.h"

namespace tyr
{
	class VulkanDevice;
	class VulkanShader final : public Shader
	{
	public:
		VulkanShader(VulkanDevice& device, const ShaderDesc& shaderDesc);
		~VulkanShader();

		VkShaderModule GetShaderModule() const { return m_ShaderModule; }

	protected:
		void Construct() override;
		ShaderBinaryLanguage GetShaderBinaryLanguage() const override { return ShaderBinaryLanguage::SpirV; };

	private:
		VulkanDevice& m_Device;
		VkShaderModule m_ShaderModule;
	};
}
