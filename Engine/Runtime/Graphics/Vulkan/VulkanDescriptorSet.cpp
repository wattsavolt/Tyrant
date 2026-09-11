#include "VulkanDescriptorSet.h"
#include "VulkanDevice.h"
#include "VulkanBuffer.h"
#include "VulkanImage.h"

namespace tyr
{
	DescriptorPoolHandle Device::CreateDescriptorPool(const DescriptorPoolDesc& desc)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		const DescriptorPoolHandle handle(device.m_DescriptorPoolPool.Create());
		DescriptorPool& pool = device.m_DescriptorPoolPool[handle.h];
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
		createInfo.flags = static_cast<VkDescriptorPoolCreateFlags>(desc.flags);
		createInfo.maxSets = desc.maxSets;
		createInfo.poolSizeCount = poolSizesCount;
		createInfo.pPoolSizes = vkPoolSizes;

		TYR_GASSERT(vkCreateDescriptorPool(device.m_LogicalDevice, &createInfo, g_VulkanAllocationCallbacks, &pool.pool));
		VulkanUtility::SetDebugName(device.m_LogicalDevice, desc.debugName, VK_OBJECT_TYPE_DESCRIPTOR_POOL, reinterpret_cast<uint64>(pool.pool));

		pool.flags = createInfo.flags;

		return handle;
	}

	void Device::DeleteDescriptorPool(DescriptorPoolHandle handle)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		DescriptorPool& pool = device.GetDescriptorPool(handle);
		vkDestroyDescriptorPool(device.m_LogicalDevice, pool.pool, g_VulkanAllocationCallbacks);
		device.m_DescriptorPoolPool.Delete(handle.h);
	}

	DescriptorSetLayoutHandle Device::CreateDescriptorSetLayout(const DescriptorSetLayoutDesc& desc)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		const DescriptorSetLayoutHandle handle(device.m_DescriptorSetLayoutPool.Create());
		DescriptorSetLayout& layout = device.m_DescriptorSetLayoutPool[handle.h];

		const uint bindingCount = desc.bindings.Size();

		// VkDescriptorBindingFlags flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT; 

		StackAllocManager stack;
		VkDescriptorBindingFlags* bindingFlags = stack.Alloc<VkDescriptorBindingFlags>(bindingCount);

		VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo =
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
			.pNext = nullptr,
			.bindingCount = bindingCount,
			.pBindingFlags = bindingFlags
		};

		VkDescriptorSetLayoutBinding* bindings = stack.Alloc<VkDescriptorSetLayoutBinding>(bindingCount);

		for (uint i = 0; i < bindingCount; ++i)
		{
			bindingFlags[i] = static_cast<VkDescriptorBindingFlags>(desc.bindings[i].bindingFlags);

			bindings[i].binding = desc.bindings[i].binding;
			bindings[i].descriptorType = VulkanUtility::ToVulkanDescriptorType(desc.bindings[i].descriptorType);
			bindings[i].descriptorCount = desc.bindings[i].descriptorCount;
			bindings[i].stageFlags = static_cast<VkShaderStageFlags>(desc.bindings[i].stageFlags);
			bindings[i].pImmutableSamplers = nullptr;

			layout.bindingDescriptorTypes.Add(bindings[i].descriptorType);
		}

		VkDescriptorSetLayoutCreateInfo createInfo;
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		createInfo.pNext = &flagsInfo;
		createInfo.flags = static_cast<VkDescriptorSetLayoutCreateFlags>(desc.flags);
		createInfo.bindingCount = bindingCount;
		createInfo.pBindings = bindings;

		TYR_GASSERT(vkCreateDescriptorSetLayout(device.m_LogicalDevice, &createInfo, g_VulkanAllocationCallbacks, &layout.layout));

#if !TYR_FINAL
		VulkanUtility::SetDebugName(device.m_LogicalDevice, desc.debugName, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, reinterpret_cast<uint64>(layout.layout));
#endif

		return handle;
	}

	void Device::DeleteDescriptorSetLayout(DescriptorSetLayoutHandle handle)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		DescriptorSetLayout& layout = device.GetDescriptorSetLayout(handle);
		vkDestroyDescriptorSetLayout(device.m_LogicalDevice, layout.layout, g_VulkanAllocationCallbacks);
		device.m_DescriptorSetLayoutPool.Delete(handle.h);
	}

	DescriptorSetHandle Device::CreateDescriptorSet(const DescriptorSetDesc& desc)
	{	
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		const DescriptorSetHandle handle(device.m_DescriptorSetPool.Create());
		DescriptorSet& set = device.m_DescriptorSetPool[handle.h];	

		DescriptorPool& pool = device.GetDescriptorPool(desc.pool);
		
		const DescriptorSetLayout& layout = device.GetDescriptorSetLayout(desc.layout);

		VkDescriptorSetAllocateInfo allocInfo;
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.pNext = nullptr;
		allocInfo.descriptorPool = pool.pool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &layout.layout;

		TYR_GASSERT(vkAllocateDescriptorSets(device.m_LogicalDevice, &allocInfo, &set.set));
		VulkanUtility::SetDebugName(device.m_LogicalDevice, desc.debugName, VK_OBJECT_TYPE_DESCRIPTOR_SET, reinterpret_cast<uint64>(set.set));

		set.pool = desc.pool;

		return handle;
	}

	void Device::DeleteDescriptorSet(DescriptorSetHandle handle)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		DescriptorSet& set = device.GetDescriptorSet(handle);
		DescriptorPool& pool = device.GetDescriptorPool(set.pool);
		// Only can delete decriptor set without resetting the pool if this flag is set. Otherwise resetting the pool is required to free it.
		if(pool.flags & VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)
		{
			vkFreeDescriptorSets(device.m_LogicalDevice, pool.pool, 1, &set.set);
		}
		device.m_DescriptorSetPool.Delete(handle.h);
	}

	void Device::UpdateDescriptorSet(DescriptorSetHandle handle, const BufferBindingUpdate* bufferUpdates, uint bufferUpdateCount, const ImageBindingUpdate* imageUpdates,
		uint imageUpdateCount, const AccelerationStructureBindingUpdate* accelerationStructureUpdates, uint accelerationStructureUpdateCount)
	{
		TYR_ASSERT((bufferUpdateCount > 0 && bufferUpdates) || (imageUpdateCount > 0 && imageUpdates)
			|| (accelerationStructureUpdateCount > 0 && accelerationStructureUpdateCount));

		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		DescriptorSet& set = device.GetDescriptorSet(handle);

		StackAllocManager stack;
		uint writeDescSetCount = bufferUpdateCount + imageUpdateCount + accelerationStructureUpdateCount;

		VkWriteDescriptorSet* writeDescSets = stack.Alloc<VkWriteDescriptorSet>(writeDescSetCount);

		uint writeDescSetIndex = 0;

		for (uint i = 0; i < bufferUpdateCount; ++i)
		{
			const BufferBindingUpdate& bufferUpdate = bufferUpdates[i];
			const uint bindingInfoCount = bufferUpdate.infoCount;
			VkDescriptorBufferInfo* const vkBufferInfos = stack.Alloc<VkDescriptorBufferInfo>(bindingInfoCount);
			const DescriptorSetLayout& layout = device.GetDescriptorSetLayout(set.layout);

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
			wds.dstSet = set.set;
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
			const uint bindingInfoCount = imageUpdate.infoCount;
			VkDescriptorImageInfo* const vkImageInfos = stack.Alloc<VkDescriptorImageInfo>(bindingInfoCount);
			const DescriptorSetLayout& layout = device.GetDescriptorSetLayout(set.layout);

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
			wds.dstSet = set.set;
			wds.dstBinding = imageUpdate.bindingIndex;
			// start index
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