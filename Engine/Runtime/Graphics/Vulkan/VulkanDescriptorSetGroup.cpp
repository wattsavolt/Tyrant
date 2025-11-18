#include "VulkanDescriptorSetGroup.h"
#include "VulkanDevice.h"
#include "VulkanBuffer.h"
#include "VulkanImage.h"

namespace tyr
{
	DescriptorPoolHandle Device::CreateDescriptorPool(const DescriptorPoolDesc& desc)
	{
		DescriptorPoolHandle handle;
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		DescriptorPool& pool = *device.m_DescriptorPoolPool.Create(handle.id);
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

		TYR_GASSERT(vkCreateDescriptorPool(device.m_LogicalDevice, &createInfo, g_VulkanAllocationCallbacks, &pool.pool));
		VulkanUtility::SetDebugName(device.m_LogicalDevice, desc.debugName.CStr(), VK_OBJECT_TYPE_DESCRIPTOR_POOL, reinterpret_cast<uint64>(pool.pool));

		return handle;
	}

	void Device::DeleteDescriptorPool(DescriptorPoolHandle handle)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		DescriptorPool& pool = device.GetDescriptorPool(handle);
		vkDestroyDescriptorPool(device.m_LogicalDevice, pool.pool, g_VulkanAllocationCallbacks);
		device.m_DescriptorPoolPool.Delete(handle.id);
	}

	DescriptorSetLayoutHandle Device::CreateDescriptorSetLayout(const DescriptorSetLayoutDesc& desc)
	{
		DescriptorSetLayoutHandle handle;
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		DescriptorSetLayout& layout = *device.m_DescriptorSetLayoutPool.Create(handle.id);

		VkDescriptorSetLayoutCreateInfo createInfo;
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		createInfo.pNext = nullptr;
		createInfo.flags = static_cast<VkDescriptorSetLayoutCreateFlags>(desc.flags);
		createInfo.bindingCount = desc.bindings.Size();

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

			layout.bindingDescriptorTypes.Add(bindings[i].descriptorType);
		}

		TYR_GASSERT(vkCreateDescriptorSetLayout(device.m_LogicalDevice, &createInfo, g_VulkanAllocationCallbacks, &layout.layout));

#if !TYR_FINAL
		VulkanUtility::SetDebugName(device.m_LogicalDevice, desc.debugName.CStr(), VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, reinterpret_cast<uint64>(layout.layout));
#endif

		return handle;
	}

	void Device::DeleteDescriptorSetLayout(DescriptorSetLayoutHandle handle)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		DescriptorSetLayout& layout = device.GetDescriptorSetLayout(handle);
		vkDestroyDescriptorSetLayout(device.m_LogicalDevice, layout.layout, g_VulkanAllocationCallbacks);
		device.m_DescriptorSetLayoutPool.Delete(handle.id);
	}

	DescriptorSetGroupHandle Device::CreateDescriptorSetGroup(const DescriptorSetGroupDesc& desc)
	{	
		DescriptorSetGroupHandle handle;
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		DescriptorSetGroup& group = *device.m_DescriptorSetGroupPool.Create(handle.id);

		const uint layoutCount = desc.layouts.Size();
		FixedArray<VkDescriptorSetLayout, c_MaxDescriptorSetsPerGroup> vkLayouts;

		DescriptorPool& pool = device.GetDescriptorPool(desc.pool);
		
		// Two sets can have same layout
		for (uint i = 0; i < layoutCount; ++i)
		{
			const DescriptorSetLayout& layout = device.GetDescriptorSetLayout(desc.layouts[i]);
			vkLayouts[i] = layout.layout;
			group.layouts.Add(desc.layouts[i]);
		}

		VkDescriptorSetAllocateInfo allocInfo;
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.pNext = nullptr;
		allocInfo.descriptorPool = pool.pool;
		allocInfo.descriptorSetCount = layoutCount;
		allocInfo.pSetLayouts = vkLayouts.Data();

		group.sets.Resize(layoutCount);

		TYR_GASSERT(vkAllocateDescriptorSets(device.m_LogicalDevice, &allocInfo, group.sets.Data()));
#if !TYR_FINAL
		for (uint i = 0; i < layoutCount; ++i)
		{
			const GDebugString debugName = desc.debugName + static_cast<int>(i);
			VulkanUtility::SetDebugName(device.m_LogicalDevice, debugName.CStr(), VK_OBJECT_TYPE_DESCRIPTOR_SET, reinterpret_cast<uint64>(group.sets[i]));
		}
#endif

		group.pool = desc.pool;

		return handle;
	}

	void Device::DeleteDescriptorSetGroup(DescriptorSetGroupHandle handle)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		DescriptorSetGroup& group = device.GetDescriptorSetGroup(handle);
		DescriptorPool& pool = device.GetDescriptorPool(group.pool);
		vkFreeDescriptorSets(device.m_LogicalDevice, pool.pool, group.sets.Size(), group.sets.Data());
		device.m_DescriptorSetGroupPool.Delete(handle.id);
	}

	void Device::UpdateDescriptorSetGroup(DescriptorSetGroupHandle handle, const BufferBindingUpdate* bufferUpdates, uint bufferUpdateCount, const ImageBindingUpdate* imageUpdates,
		uint imageUpdateCount, const AccelerationStructureBindingUpdate* accelerationStructureUpdates, uint accelerationStructureUpdateCount)
	{
		TYR_ASSERT((bufferUpdateCount > 0 && bufferUpdates) || (imageUpdateCount > 0 && imageUpdates)
			|| (accelerationStructureUpdateCount > 0 && accelerationStructureUpdateCount));

		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		DescriptorSetGroup& group = device.GetDescriptorSetGroup(handle);

		StackAllocManager stack;
		uint writeDescSetCount = bufferUpdateCount + imageUpdateCount + accelerationStructureUpdateCount;

		VkWriteDescriptorSet* writeDescSets = stack.Alloc<VkWriteDescriptorSet>(writeDescSetCount);

		uint writeDescSetIndex = 0;

		for (uint i = 0; i < bufferUpdateCount; ++i)
		{
			const BufferBindingUpdate& bufferUpdate = bufferUpdates[i];
			const uint bindingInfoCount = static_cast<uint>(bufferUpdate.bufferBindingInfos.Size());
			VkDescriptorBufferInfo* const vkBufferInfos = stack.Alloc<VkDescriptorBufferInfo>(bindingInfoCount);
			const DescriptorSetLayout& layout = device.GetDescriptorSetLayout(group.layouts[bufferUpdate.descriptorIndex]);

			for (uint j = 0; j < bindingInfoCount; ++j)
			{
				const BufferBindingInfo& bindingInfo = bufferUpdate.bufferBindingInfos[j];
				const BufferView& bufferView = device.GetBufferView(bindingInfo.bufferView);
				const Buffer& buffer = device.GetBuffer(bufferView.buffer);
				vkBufferInfos[j].buffer = buffer.buffer;
				vkBufferInfos[j].offset = bufferView.offset;
				vkBufferInfos[j].range = bufferView.size;
			}

			VkWriteDescriptorSet& wds = writeDescSets[writeDescSetIndex++];
			wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			wds.pNext = nullptr;
			wds.dstSet = group.sets[bufferUpdate.descriptorIndex];
			wds.dstBinding = bufferUpdate.bindingIndex;
			wds.dstArrayElement = bufferUpdate.descriptorArrayIndex;
			wds.descriptorCount = bindingInfoCount;
			wds.descriptorType = layout.bindingDescriptorTypes[bufferUpdate.bindingIndex];
			wds.pImageInfo = nullptr;
			wds.pBufferInfo = vkBufferInfos;
			wds.pTexelBufferView = nullptr;
		}
			
		for (uint i = 0; i < imageUpdateCount; ++i)
		{
			const ImageBindingUpdate& imageUpdate = imageUpdates[i];
			const uint bindingInfoCount = static_cast<uint>(imageUpdate.imageBindingInfos.Size());
			VkDescriptorImageInfo* const vkImageInfos = stack.Alloc<VkDescriptorImageInfo>(bindingInfoCount);
			const DescriptorSetLayout& layout = device.GetDescriptorSetLayout(group.layouts[imageUpdate.descriptorIndex]);

			for (uint j = 0; j < bindingInfoCount; ++j)
			{
				const ImageBindingInfo& bindingInfo = imageUpdate.imageBindingInfos[j];
				const ImageView& imageView = device.GetImageView(bindingInfo.imageView);
				const Image& image = device.GetImage(imageView.image);

				vkImageInfos[j].imageView = imageView.imageView;
				vkImageInfos[j].imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				if (bindingInfo.hasSampler)
				{
					const Sampler& sampler = device.GetSampler(bindingInfo.sampler);
					vkImageInfos[j].sampler = sampler.sampler;
				}
				else
				{
					vkImageInfos[j].sampler = nullptr;
				}
			}

			VkWriteDescriptorSet& wds = writeDescSets[writeDescSetIndex++];
			wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			wds.pNext = nullptr;
			wds.dstSet = group.sets[imageUpdate.descriptorIndex];
			wds.dstBinding = imageUpdate.bindingIndex;
			wds.dstArrayElement = imageUpdate.descriptorArrayIndex;
			wds.descriptorCount = bindingInfoCount;
			wds.descriptorType = layout.bindingDescriptorTypes[imageUpdate.bindingIndex];
			wds.pImageInfo = vkImageInfos;
			wds.pBufferInfo = nullptr;
			wds.pTexelBufferView = nullptr;
		}
		
		if (accelerationStructureUpdateCount > 0)
		{
			// TODO
		}

		vkUpdateDescriptorSets(device.m_LogicalDevice, writeDescSetCount, writeDescSets, 0, nullptr);
	}
}