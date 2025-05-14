#include "VulkanDescriptorSet.h"
#include "VulkanDevice.h"
#include "VulkanBuffer.h"
#include "VulkanImage.h"

namespace tyr
{
	VulkanDescriptorPool::VulkanDescriptorPool(VulkanDevice& device, const DescriptorPoolDesc& desc, const char* debugName)
		: m_Device(device)
		, DescriptorPool(desc)
	{
		StackAllocManager stack;
		const uint poolSizesCount = static_cast<uint>(desc.poolSizes.Size());;
		VkDescriptorPoolSize* vkPoolSizes = stack.Alloc<VkDescriptorPoolSize>(poolSizesCount);
		for (uint i = 0; i < poolSizesCount; ++i)
		{
			vkPoolSizes[i].type = VulkanUtility::ToVulkanDescriptorType(desc.poolSizes[i].descriptorType);
			vkPoolSizes[i].descriptorCount = desc.poolSizes[i].descriptorCount;
		}

		VkDescriptorPoolCreateInfo createInfo;
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		createInfo.pNext = nullptr;
		createInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		createInfo.maxSets = desc.maxSets;
		createInfo.poolSizeCount = poolSizesCount;
		createInfo.pPoolSizes = vkPoolSizes;

		TYR_GASSERT(vkCreateDescriptorPool(device.GetLogicalDevice(), &createInfo, g_VulkanAllocationCallbacks, &m_Pool));
		VulkanUtility::SetDebugName(device.GetLogicalDevice(), debugName, VK_OBJECT_TYPE_DESCRIPTOR_POOL, reinterpret_cast<uint64>(m_Pool));
	}

	VulkanDescriptorPool::~VulkanDescriptorPool()
	{
		vkDestroyDescriptorPool(m_Device.GetLogicalDevice(), m_Pool, g_VulkanAllocationCallbacks);
	}

	VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(VulkanDevice& device, const DescriptorSetLayoutDesc& desc, const char* debugName)
		: m_Device(device)
		, DescriptorSetLayout(desc)
	{
		VkDescriptorSetLayoutCreateInfo createInfo;
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		createInfo.pNext = nullptr;
		createInfo.flags = static_cast<VkDescriptorSetLayoutCreateFlags>(desc.flags);
		createInfo.bindingCount = desc.bindingCount;;

		StackAllocManager stack;
		VkDescriptorSetLayoutBinding* bindings = stack.Alloc<VkDescriptorSetLayoutBinding>(createInfo.bindingCount);
		createInfo.pBindings = bindings;

		for (uint i = 0; i < createInfo.bindingCount; ++i)
		{
			bindings[i].binding = desc.bindings[i].binding;
			bindings[i].descriptorType = VulkanUtility::ToVulkanDescriptorType(desc.bindings[i].descriptorType);
			bindings[i].descriptorCount = desc.bindings[i].descriptorCount;
			bindings[i].stageFlags = static_cast<VkShaderStageFlags>(desc.bindings[i].stageFlags);
			bindings[i].pImmutableSamplers = nullptr;
		}

		TYR_GASSERT(vkCreateDescriptorSetLayout(m_Device.GetLogicalDevice(), &createInfo, g_VulkanAllocationCallbacks, &m_Layout));
		VulkanUtility::SetDebugName(device.GetLogicalDevice(), debugName, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, reinterpret_cast<uint64>(m_Layout));
	}

	VulkanDescriptorSetLayout::~VulkanDescriptorSetLayout()
	{
		vkDestroyDescriptorSetLayout(m_Device.GetLogicalDevice(), m_Layout, g_VulkanAllocationCallbacks);
	}

	VulkanDescriptorSetGroup::VulkanDescriptorSetGroup(VulkanDevice& device, const DescriptorSetGroupDesc& desc, const char* debugName)
		: DescriptorSetGroup(desc)
		, m_Device(device)
	{	
		VkDescriptorSetLayout* vkLayouts = StackAlloc<VkDescriptorSetLayout>(desc.layoutCount);

		// Add duplicate layouts per set
		for (uint i = 0; i < desc.layoutCount; ++i)
		{
			const Ref<VulkanDescriptorSetLayout>& layout = RefCast<VulkanDescriptorSetLayout>(desc.layouts[i]);
			vkLayouts[i] = layout->GetLayout();
		}

		m_Pool = RefCast<VulkanDescriptorPool>(desc.pool)->GetPool();

		VkDescriptorSetAllocateInfo allocInfo;
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.pNext = nullptr;
		allocInfo.descriptorPool = m_Pool;
		allocInfo.descriptorSetCount = desc.layoutCount;
		allocInfo.pSetLayouts = vkLayouts;

		TYR_GASSERT(vkAllocateDescriptorSets(m_Device.GetLogicalDevice(), &allocInfo, m_Sets));
		StackFreeLast();

#if TYR_DEBUG
		const String debugStrStart = String(debugName) + "_";
		for (uint i = 0; i < desc.layoutCount; ++i)
		{
			const String debugStr = debugStrStart + std::to_string(i);
			VulkanUtility::SetDebugName(device.GetLogicalDevice(), debugStrStart.c_str(), VK_OBJECT_TYPE_DESCRIPTOR_SET, reinterpret_cast<uint64>(m_Sets[i]));
		}
#endif
	}

	VulkanDescriptorSetGroup::~VulkanDescriptorSetGroup()
	{
		vkFreeDescriptorSets(m_Device.GetLogicalDevice(), m_Pool, m_Desc.layoutCount, m_Sets);
	}

	void VulkanDescriptorSetGroup::Update(const BufferBindingUpdate* bufferUpdates, uint bufferUpdateCount, const ImageBindingUpdate* imageUpdates,
		uint imageUpdateCount, const AccelerationStructureBindingUpdate* accelerationStructureUpdates, uint accelerationStructureUpdateCount)
	{
		TYR_ASSERT((bufferUpdateCount > 0 && bufferUpdates) || (imageUpdateCount > 0 && imageUpdates)
			|| (accelerationStructureUpdateCount > 0 && accelerationStructureUpdateCount));

		StackAllocManager stack;
		uint writeDescSetCount = bufferUpdateCount + imageUpdateCount + accelerationStructureUpdateCount;

		VkWriteDescriptorSet* writeDescSets = stack.Alloc<VkWriteDescriptorSet>(writeDescSetCount);

		uint writeDescSetIndex = 0;

		if (bufferUpdateCount > 0)
		{
			for (uint i = 0; i < bufferUpdateCount; ++i)
			{
				const BufferBindingUpdate& bufferUpdate = bufferUpdates[i];
				const uint bindingInfoCount = static_cast<uint>(bufferUpdate.bufferBindingInfos.Size());
				VkDescriptorBufferInfo* const vkBufferInfos = stack.Alloc<VkDescriptorBufferInfo>(bindingInfoCount);
				const Ref<VulkanDescriptorSetLayout>& layout = RefCast<VulkanDescriptorSetLayout>(m_Desc.layouts[bufferUpdate.descriptorIndex]);

				for (uint j = 0; j < bindingInfoCount; ++j)
				{
					const BufferBindingInfo& bindingInfo = bufferUpdate.bufferBindingInfos[j];
					const SRef<BufferView>& bufferView = bindingInfo.bufferView;
					const BufferViewDesc& viewDesc = bufferView->GetDesc();
					const VulkanBuffer* buffer = viewDesc.buffer.GetAs<VulkanBuffer>();
					vkBufferInfos[j].buffer = buffer->GetBuffer();
					vkBufferInfos[j].offset = viewDesc.offset;
					vkBufferInfos[j].range = viewDesc.size;
				}

				VkWriteDescriptorSet& wds = writeDescSets[writeDescSetIndex++];
				wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				wds.pNext = nullptr;
				wds.dstSet = m_Sets[bufferUpdate.descriptorIndex];
				wds.dstBinding = bufferUpdate.bindingIndex;
				wds.dstArrayElement = bufferUpdate.descriptorArrayIndex;
				wds.descriptorCount = bindingInfoCount;
				wds.descriptorType = static_cast<VkDescriptorType>(layout->GetDesc().bindings[bufferUpdate.bindingIndex].descriptorType);
				wds.pImageInfo = nullptr;
				wds.pBufferInfo = vkBufferInfos;
				wds.pTexelBufferView = nullptr;
			}
		}
		
		for (uint i = 0; i < imageUpdateCount; ++i)
		{
			const ImageBindingUpdate& imageUpdate = imageUpdates[i];
			const uint bindingInfoCount = static_cast<uint>(imageUpdate.imageBindingInfos.Size());
			VkDescriptorImageInfo* const vkImageInfos = stack.Alloc<VkDescriptorImageInfo>(bindingInfoCount);
			const Ref<VulkanDescriptorSetLayout>& layout = RefCast<VulkanDescriptorSetLayout>(m_Desc.layouts[imageUpdate.descriptorIndex]);

			for (uint j = 0; j < bindingInfoCount; ++j)
			{
				const ImageBindingInfo& bindingInfo = imageUpdate.imageBindingInfos[j];
				const VulkanImageView* imageView = bindingInfo.imageView.GetAs<VulkanImageView>();
				const ImageViewDesc& viewDesc = imageView->GetDesc();
				const VulkanImage* image = viewDesc.image.GetAs<VulkanImage>();
				const ImageDesc& imageDesc = image->GetDesc();

				vkImageInfos[j].imageView = imageView->GetImageView();
				vkImageInfos[j].imageLayout = VulkanUtility::ToVulkanImageLayout(imageDesc.layout);
				if (bindingInfo.sampler)
				{
					vkImageInfos[j].sampler = RefCast<VulkanSampler>(bindingInfo.sampler)->GetSampler();
				}
				else
				{
					vkImageInfos[j].sampler = nullptr;
				}
			}

			VkWriteDescriptorSet& wds = writeDescSets[writeDescSetIndex++];
			wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			wds.pNext = nullptr;
			wds.dstSet = m_Sets[imageUpdate.descriptorIndex];
			wds.dstBinding = imageUpdate.bindingIndex;
			wds.dstArrayElement = imageUpdate.descriptorArrayIndex;
			wds.descriptorCount = bindingInfoCount;
			wds.descriptorType = static_cast<VkDescriptorType>(layout->GetDesc().bindings[imageUpdate.bindingIndex].descriptorType);
			wds.pImageInfo = vkImageInfos;
			wds.pBufferInfo = nullptr;
			wds.pTexelBufferView = nullptr;
		}
		
		if (accelerationStructureUpdateCount > 0)
		{
			// TODO
		}

		vkUpdateDescriptorSets(m_Device.GetLogicalDevice(), writeDescSetCount, writeDescSets, 0, nullptr);
	}
}