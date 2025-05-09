
#pragma once

#include "RenderAPI/DescriptorSet.h"
#include <vulkan/vulkan.h>

namespace tyr
{
	class VulkanDevice;

	class VulkanDescriptorPool final : public DescriptorPool
	{
	public:
		VulkanDescriptorPool(VulkanDevice& device, const DescriptorPoolDesc& desc, const char* debugName);
		~VulkanDescriptorPool();

		VkDescriptorPool GetPool() const { return m_Pool; }

	private:
		VkDescriptorPool m_Pool;
		VulkanDevice& m_Device;
	};

	class VulkanDescriptorSetLayout final : public DescriptorSetLayout
	{
	public:
		VulkanDescriptorSetLayout(VulkanDevice& device, const DescriptorSetLayoutDesc& desc, const char* debugName);
		~VulkanDescriptorSetLayout();

		VkDescriptorSetLayout GetLayout() const { return m_Layout; }

	private:
		VkDescriptorSetLayout m_Layout;
		VulkanDevice& m_Device;
	};

	class VulkanDescriptorSetGroup final : public DescriptorSetGroup
	{
	public:
		VulkanDescriptorSetGroup(VulkanDevice& device, const DescriptorSetGroupDesc& desc, const char* debugName);
		~VulkanDescriptorSetGroup();

		void Update(const BufferBindingUpdate* bufferUpdates, uint bufferUpdateCount, const ImageBindingUpdate* imageUpdates = nullptr,
			uint imageUpdateCount = 0, const AccelerationStructureBindingUpdate* accelerationStructureUpdates = nullptr,
			uint accelerationStructureUpdateCount = 0) override;

		VkDescriptorSet* GetSets() { return m_Sets; }
		uint GetSetCount() const { return m_Desc.layoutCount; }

	private:
		VkDescriptorSet m_Sets[c_MaxDescriptorSetsPerGroup];
		VulkanDevice& m_Device;
		VkDescriptorPool m_Pool;
	};
}
