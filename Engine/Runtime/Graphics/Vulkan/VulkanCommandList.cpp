

#include "VulkanCommandList.h"
#include "Memory/StackAllocation.h"
#include "VulkanCommandAllocator.h"
#include "VulkanSync.h"
#include "VulkanBuffer.h"
#include "VulkanBuffer.h"
#include "VulkanImage.h"
#include "VulkanDescriptorSet.h"
#include "VulkanPipeline.h"
#include "VulkanExtensions.h"

namespace tyr
{
	VulkanCommandList::VulkanCommandList(VulkanDevice& device, const CommandListDesc& desc)
		: CommandList(desc)
		, m_Device(device)
	{
		m_CommandPool = RefCast<VulkanCommandAllocator>(desc.allocator)->GetCommandPool();
		m_QueueType = m_Desc.allocator->GetDesc().queueType;
		m_QueueFamilyIndex = device.GetQueueFamilyIndex(m_QueueType);
		VkCommandBufferAllocateInfo info;
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		info.pNext = nullptr;
		info.commandPool = m_CommandPool;
		info.level = VulkanUtility::ToVulkanCommandBufferLevel(desc.type);
		info.commandBufferCount = 1;

		vkAllocateCommandBuffers(device.GetLogicalDevice(), &info, &m_CommandBuffer);
		VulkanUtility::SetDebugName(device.GetLogicalDevice(), desc.debugName.c_str(), VK_OBJECT_TYPE_COMMAND_BUFFER, reinterpret_cast<uint64>(m_CommandBuffer));
	}

	VulkanCommandList::~VulkanCommandList()
	{
		vkFreeCommandBuffers(m_Device.GetLogicalDevice(), m_CommandPool, 1, &m_CommandBuffer);
	}
	
	void VulkanCommandList::Begin(CommandBufferUsage usage)
	{
		VkCommandBufferBeginInfo info;
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		info.pNext = nullptr;
		info.flags = static_cast<VkCommandBufferUsageFlags>(usage);
		info.pInheritanceInfo = nullptr;

		TYR_GASSERT(vkBeginCommandBuffer(m_CommandBuffer, &info));
	}

	void VulkanCommandList::End()
	{
		TYR_GASSERT(vkEndCommandBuffer(m_CommandBuffer));
	}

	void VulkanCommandList::Reset(bool releaseResources)
	{
		TYR_GASSERT(vkResetCommandBuffer(m_CommandBuffer, releaseResources ? VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT : 0));
	}

	void VulkanCommandList::ClearColourImage(SRef<Image>& image, ImageLayout layout, const ClearColourValue& clearValue, const SubresourceRange* subresourceRanges, uint rangeCount)
	{
		VkImageSubresourceRange* vkSubresourceRanges = StackAlloc<VkImageSubresourceRange>(rangeCount);
		for (uint i = 0; i < rangeCount; ++i)
		{
			vkSubresourceRanges[i].aspectMask = static_cast<VkImageAspectFlagBits>(subresourceRanges[i].aspect);
			vkSubresourceRanges[i].baseMipLevel = subresourceRanges[i].baseMipLevel;
			vkSubresourceRanges[i].levelCount = subresourceRanges[i].mipLevelCount;
			vkSubresourceRanges[i].baseArrayLayer = subresourceRanges[i].baseArrayLayer;
			vkSubresourceRanges[i].layerCount = subresourceRanges[i].arrayLayerCount;
		}
		VkClearColorValue* vkClearValue = (VkClearColorValue*)&clearValue;
		vkCmdClearColorImage(m_CommandBuffer, image.GetAs<VulkanImage>()->GetImage(), static_cast<VkImageLayout>(layout), vkClearValue, rangeCount, vkSubresourceRanges);
		StackFreeLast();
	}

	void VulkanCommandList::ClearDepthStencilImage(SRef<Image>& image, ImageLayout layout, const ClearDepthStencilValue& clearValue, const SubresourceRange* subresourceRanges, uint rangeCount)
	{
		VkImageSubresourceRange* vkSubresourceRanges = StackAlloc<VkImageSubresourceRange>(rangeCount);
		for (uint i = 0; i < rangeCount; ++i)
		{
			vkSubresourceRanges[i].aspectMask = static_cast<VkImageAspectFlagBits>(subresourceRanges[i].aspect);
			vkSubresourceRanges[i].baseMipLevel = subresourceRanges[i].baseMipLevel;
			vkSubresourceRanges[i].levelCount = subresourceRanges[i].mipLevelCount;
			vkSubresourceRanges[i].baseArrayLayer = subresourceRanges[i].baseArrayLayer;
			vkSubresourceRanges[i].layerCount = subresourceRanges[i].arrayLayerCount;
		}
		VkClearDepthStencilValue* vkClearValue = (VkClearDepthStencilValue*)&clearValue;
		vkCmdClearDepthStencilImage(m_CommandBuffer, image.GetAs<VulkanImage>()->GetImage(), static_cast<VkImageLayout>(layout), vkClearValue, rangeCount, vkSubresourceRanges);
		StackFreeLast();
	}

	void VulkanCommandList::BeginRendering(const RenderingInfo& renderingInfo) 
	{
		auto CreateVulkankRenderingAttachmentInfo = [](const RenderingAttachmentInfo& renderingAttachmentInfo, 
			VkRenderingAttachmentInfo& vkRenderingAttachmentInfo) -> void
		{
			vkRenderingAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			vkRenderingAttachmentInfo.pNext = nullptr;
			vkRenderingAttachmentInfo.imageView = renderingAttachmentInfo.imageView.GetAs<VulkanImageView>()->GetImageView();
			vkRenderingAttachmentInfo.imageLayout = VulkanUtility::ToVulkanImageLayout(renderingAttachmentInfo.imageLayout);
			vkRenderingAttachmentInfo.resolveMode = static_cast<VkResolveModeFlagBits>(renderingAttachmentInfo.resolveMode);
			if (renderingAttachmentInfo.resolveImageView)
			{
				vkRenderingAttachmentInfo.resolveImageView = renderingAttachmentInfo.resolveImageView.GetAs<VulkanImageView>()->GetImageView();
				vkRenderingAttachmentInfo.resolveImageLayout = VulkanUtility::ToVulkanImageLayout(renderingAttachmentInfo.resolveImageLayout);
			}
			else
			{
				vkRenderingAttachmentInfo.resolveImageView = VK_NULL_HANDLE;
				vkRenderingAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			}
			vkRenderingAttachmentInfo.loadOp = VulkanUtility::ToVulkanAttachmentLoadOp(renderingAttachmentInfo.loadOp);
			vkRenderingAttachmentInfo.storeOp = VulkanUtility::ToVulkanAttachmentStoreOp(renderingAttachmentInfo.storeOp);
			vkRenderingAttachmentInfo.clearValue = *reinterpret_cast<const VkClearValue*>(&renderingAttachmentInfo.clearValue);
		};

		StackAllocManager stack;

		const uint colourAttachmentCount = static_cast<uint>(renderingInfo.colourAttachments.Size());
		VkRenderingAttachmentInfo* vkColourAttachments = stack.Alloc<VkRenderingAttachmentInfo>(colourAttachmentCount);

		for (uint i = 0; i < colourAttachmentCount; ++i)
		{
			CreateVulkankRenderingAttachmentInfo(renderingInfo.colourAttachments[i], vkColourAttachments[i]);
		}

		VkRenderingAttachmentInfo* vkDepthAttachment = nullptr;
		if (renderingInfo.depthAttachment)
		{
			vkDepthAttachment = stack.Alloc<VkRenderingAttachmentInfo>();
			CreateVulkankRenderingAttachmentInfo(*renderingInfo.depthAttachment, *vkDepthAttachment);
		}

		VkRenderingAttachmentInfo* vkStencilAttachment = nullptr;
		if (renderingInfo.stencilAttachment)
		{
			vkStencilAttachment = stack.Alloc<VkRenderingAttachmentInfo>();
			CreateVulkankRenderingAttachmentInfo(*renderingInfo.stencilAttachment, *vkStencilAttachment);
		}

		VkRenderingInfo vkRenderingInfo;
		vkRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		vkRenderingInfo.pNext = nullptr;
		vkRenderingInfo.flags = static_cast<VkRenderingFlags>(renderingInfo.flags);
		vkRenderingInfo.renderArea.offset.x = renderingInfo.renderArea.offset.x;
		vkRenderingInfo.renderArea.offset.y = renderingInfo.renderArea.offset.y;
		vkRenderingInfo.renderArea.extent.width = renderingInfo.renderArea.extents.width;
		vkRenderingInfo.renderArea.extent.height = renderingInfo.renderArea.extents.height;
		vkRenderingInfo.layerCount = renderingInfo.layerCount;
		vkRenderingInfo.viewMask = renderingInfo.viewMask;
		vkRenderingInfo.colorAttachmentCount = colourAttachmentCount;
		vkRenderingInfo.pColorAttachments = vkColourAttachments;
		vkRenderingInfo.pDepthAttachment = vkDepthAttachment;
		vkRenderingInfo.pStencilAttachment = vkStencilAttachment;

		vkCmdBeginRendering(m_CommandBuffer, &vkRenderingInfo);
	}

	void VulkanCommandList::EndRendering() 
	{
		vkCmdEndRendering(m_CommandBuffer);
	}

	void VulkanCommandList::AddBarriers(const BufferBarrier* bufferBarriers, uint bufferBarrierCount,
		const ImageBarrier* imageBarriers, uint imageBarrierCount,
		const PipelineBarrier* pipelineBarriers, uint pipelineBarrierCount,
		Dependency dependency)
	{
		StackAllocManager stack;
		VkMemoryBarrier2* vkMemoryBarriers = nullptr;
		if (pipelineBarrierCount > 0)
		{
			vkMemoryBarriers = stack.Alloc<VkMemoryBarrier2>(pipelineBarrierCount);
			for (uint i = 0; i < pipelineBarrierCount; ++i)
			{
				vkMemoryBarriers[i].sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
				vkMemoryBarriers[i].pNext = nullptr;
				vkMemoryBarriers[i].srcAccessMask = static_cast<VkAccessFlags2>(pipelineBarriers[i].srcAccess);
				vkMemoryBarriers[i].dstAccessMask = static_cast<VkAccessFlags2>(pipelineBarriers[i].dstAccess);
				vkMemoryBarriers[i].srcStageMask = static_cast<VkPipelineStageFlags2>(pipelineBarriers[i].srcStage);
				vkMemoryBarriers[i].dstStageMask = static_cast<VkPipelineStageFlags2>(pipelineBarriers[i].dstStage);
			}
		}

		VkBufferMemoryBarrier2* vkBufferBarriers = nullptr;
		if (bufferBarrierCount > 0)
		{
			vkBufferBarriers = stack.Alloc<VkBufferMemoryBarrier2>(bufferBarrierCount);
			for (uint i = 0; i < bufferBarrierCount; ++i)
			{
				VulkanBuffer* buffer = static_cast<VulkanBuffer*>(bufferBarriers[i].buffer);
				vkBufferBarriers[i].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
				vkBufferBarriers[i].pNext = nullptr;
				vkBufferBarriers[i].srcAccessMask = static_cast<VkAccessFlags2>(bufferBarriers[i].srcAccess);
				vkBufferBarriers[i].dstAccessMask = static_cast<VkAccessFlags2>(bufferBarriers[i].dstAccess);
				vkBufferBarriers[i].buffer = buffer->GetBuffer();
				vkBufferBarriers[i].offset = bufferBarriers[i].offset;
				vkBufferBarriers[i].size = buffer->GetDesc().size;
				vkBufferBarriers[i].srcQueueFamilyIndex = bufferBarriers[i].srcQueueFamilyIndex;
				if (vkBufferBarriers[i].srcQueueFamilyIndex != VK_QUEUE_FAMILY_IGNORED)
				{
					vkBufferBarriers[i].dstQueueFamilyIndex = m_QueueFamilyIndex;
				}
				else
				{
					vkBufferBarriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				}
				vkBufferBarriers[i].srcStageMask = static_cast<VkPipelineStageFlags2>(bufferBarriers[i].srcStage);
				vkBufferBarriers[i].dstStageMask = static_cast<VkPipelineStageFlags2>(bufferBarriers[i].dstStage);
			}
		}

		VkImageMemoryBarrier2* vkImageBarriers = nullptr;
		if (imageBarrierCount > 0)
		{
			vkImageBarriers = stack.Alloc<VkImageMemoryBarrier2>(imageBarrierCount);
			for (uint i = 0; i < imageBarrierCount; ++i)
			{
				VulkanImage* image = static_cast<VulkanImage*>(imageBarriers[i].image);
				vkImageBarriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
				vkImageBarriers[i].pNext = nullptr;
				vkImageBarriers[i].srcAccessMask = static_cast<VkAccessFlags2>(imageBarriers[i].srcAccess);
				vkImageBarriers[i].dstAccessMask = static_cast<VkAccessFlags2>(imageBarriers[i].dstAccess);
				vkImageBarriers[i].image = image->GetImage();
				vkImageBarriers[i].srcQueueFamilyIndex = imageBarriers[i].srcQueueFamilyIndex;
				if (vkImageBarriers[i].srcQueueFamilyIndex != VK_QUEUE_FAMILY_IGNORED)
				{
					vkImageBarriers[i].dstQueueFamilyIndex = m_QueueFamilyIndex;
				}
				else
				{
					vkImageBarriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				}
				vkImageBarriers[i].subresourceRange.aspectMask = static_cast<VkImageAspectFlagBits>(imageBarriers[i].subresourceRange.aspect);
				vkImageBarriers[i].subresourceRange.baseMipLevel = imageBarriers[i].subresourceRange.baseMipLevel;
				vkImageBarriers[i].subresourceRange.levelCount = imageBarriers[i].subresourceRange.mipLevelCount;
				vkImageBarriers[i].subresourceRange.baseArrayLayer = imageBarriers[i].subresourceRange.baseArrayLayer;
				vkImageBarriers[i].subresourceRange.layerCount = imageBarriers[i].subresourceRange.arrayLayerCount;
				vkImageBarriers[i].oldLayout = VulkanUtility::ToVulkanImageLayout(imageBarriers[i].srcLayout);
				vkImageBarriers[i].newLayout = VulkanUtility::ToVulkanImageLayout(imageBarriers[i].dstLayout);
				vkImageBarriers[i].srcStageMask = static_cast<VkPipelineStageFlags2>(imageBarriers[i].srcStage);
				vkImageBarriers[i].dstStageMask = static_cast<VkPipelineStageFlags2>(imageBarriers[i].dstStage);
			}
		}

		VkDependencyInfo vkDependencyInfo;
		vkDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		vkDependencyInfo.pNext = nullptr;
		vkDependencyInfo.dependencyFlags = static_cast<VkDependencyFlags>(dependency);
		vkDependencyInfo.memoryBarrierCount = static_cast<uint32_t>(pipelineBarrierCount);
		vkDependencyInfo.pMemoryBarriers = vkMemoryBarriers;
		vkDependencyInfo.bufferMemoryBarrierCount = static_cast<uint>(bufferBarrierCount);
		vkDependencyInfo.pBufferMemoryBarriers = vkBufferBarriers;
		vkDependencyInfo.imageMemoryBarrierCount = static_cast<uint>(imageBarrierCount);
		vkDependencyInfo.pImageMemoryBarriers = vkImageBarriers;

		vkCmdPipelineBarrier2(m_CommandBuffer, &vkDependencyInfo);
	}

	void VulkanCommandList::CopyBuffer(const SRef<Buffer>& srcBuffer, const SRef<Buffer>& dstBuffer, const BufferCopyInfo* copyInfos, uint copyInfoCount)
	{
		VkBufferCopy* copies = StackAlloc<VkBufferCopy>(copyInfoCount);
		for (uint i = 0; i < copyInfoCount; ++i)
		{
			copies[i].srcOffset = static_cast<VkDeviceSize>(copyInfos[i].srcOffset);
			copies[i].dstOffset = static_cast<VkDeviceSize>(copyInfos[i].dstOffset);
			copies[i].size = static_cast<VkDeviceSize>(copyInfos[i].size);
		}
		vkCmdCopyBuffer(m_CommandBuffer, srcBuffer.GetAs<VulkanBuffer>()->GetBuffer(), dstBuffer.GetAs<VulkanBuffer>()->GetBuffer(), copyInfoCount, copies);
		StackFreeLast();
	}

	void VulkanCommandList::CopyBuffer(const SRef<Buffer>& srcBuffer, const SRef<Buffer>& dstBuffer)
	{
		VkBufferCopy vkBufferCopy;
		vkBufferCopy.srcOffset = 0;
		vkBufferCopy.dstOffset = 0;
		vkBufferCopy.size = static_cast<VkDeviceSize>(srcBuffer->GetDesc().size);
		vkCmdCopyBuffer(m_CommandBuffer, srcBuffer.GetAs<VulkanBuffer>()->GetBuffer(), dstBuffer.GetAs<VulkanBuffer>()->GetBuffer(), 1, &vkBufferCopy);
	}

	void VulkanCommandList::CopyBufferToImage(const SRef<Buffer>& buffer, const SRef<Image>& image, ImageLayout targetLayout, const BufferImageCopyInfo* regions, uint regionCount)
	{
		VkBufferImageCopy* copies = StackAlloc<VkBufferImageCopy>(regionCount);
		for (uint i = 0; i < regionCount; ++i)
		{
			const BufferImageCopyInfo& region = regions[i];
			VkBufferImageCopy& copy = copies[i];
			copy.bufferOffset = static_cast<VkDeviceSize>(region.bufferOffset);
			copy.bufferRowLength = region.bufferRowLength;
			copy.bufferImageHeight = region.bufferImageHeight;
			copy.imageSubresource = { static_cast<VkImageAspectFlags>(region.imageSubresource.aspect), region.imageSubresource.mipLevel, region.imageSubresource.baseArrayLayer, region.imageSubresource.arrayLayerCount };
			copy.imageOffset = { region.imageOffset.x, region.imageOffset.y, region.imageOffset.z };
			copy.imageExtent = { region.imageExtent.width, region.imageExtent.height, region.imageExtent.depth };
		}
		VkImageLayout vkLayout = VulkanUtility::ToVulkanImageLayout(targetLayout);
		vkCmdCopyBufferToImage(m_CommandBuffer, buffer.GetAs<VulkanBuffer>()->GetBuffer(), image.GetAs<VulkanImage>()->GetImage(), vkLayout, regionCount, copies);
		StackFreeLast();
	}

	void VulkanCommandList::CopyImage(const SRef<Image>& srcImage, ImageLayout srcLayout, const SRef<Image>& dstImage, ImageLayout dstLayout, const ImageCopyInfo* regions, uint regionCount)
	{
		VkImageCopy* copies = StackAlloc<VkImageCopy>(regionCount);
		for (uint i = 0; i < regionCount; ++i)
		{
			const ImageCopyInfo& region = regions[i];
			VkImageCopy& copy = copies[i];
			copy.srcSubresource = { static_cast<VkImageAspectFlags>(region.srcSubresource.aspect), region.srcSubresource.mipLevel, region.srcSubresource.baseArrayLayer, region.srcSubresource.arrayLayerCount };
			copy.srcOffset = { region.srcOffset.x, region.srcOffset.y, region.srcOffset.z };
			copy.dstSubresource = { static_cast<VkImageAspectFlags>(region.dstSubresource.aspect), region.dstSubresource.mipLevel, region.dstSubresource.baseArrayLayer, region.dstSubresource.arrayLayerCount };
			copy.dstOffset = { region.dstOffset.x, region.dstOffset.y, region.dstOffset.z };
			copy.extent = { region.extent.width, region.extent.height, region.extent.depth };
		}
		VkImageLayout vkSrcLayout = VulkanUtility::ToVulkanImageLayout(srcLayout);
		VkImageLayout vkDstLayout = VulkanUtility::ToVulkanImageLayout(dstLayout);
		vkCmdCopyImage(m_CommandBuffer, srcImage.GetAs<VulkanImage>()->GetImage(), vkSrcLayout, dstImage.GetAs<VulkanImage>()->GetImage(), vkDstLayout, regionCount, copies);
		StackFreeLast();
	}

	void VulkanCommandList::CopyImageToBuffer(const SRef<Image>& image, const SRef<Buffer>& buffer, ImageLayout currentLayout, const BufferImageCopyInfo* regions, uint regionCount)
	{
		VkBufferImageCopy* copies = StackAlloc<VkBufferImageCopy>(regionCount);
		for (uint i = 0; i < regionCount; ++i)
		{
			const BufferImageCopyInfo& region = regions[i];
			VkBufferImageCopy& copy = copies[i];
			copy.bufferOffset = static_cast<VkDeviceSize>(region.bufferOffset);
			copy.bufferRowLength = region.bufferRowLength;
			copy.bufferImageHeight = region.bufferImageHeight;
			copy.imageSubresource = { static_cast<VkImageAspectFlags>(region.imageSubresource.aspect), region.imageSubresource.mipLevel, region.imageSubresource.baseArrayLayer, region.imageSubresource.arrayLayerCount };
			copy.imageOffset = { region.imageOffset.x, region.imageOffset.y, region.imageOffset.z };
			copy.imageExtent = { region.imageExtent.width, region.imageExtent.height, region.imageExtent.depth };
		}
		VkImageLayout vkLayout = VulkanUtility::ToVulkanImageLayout(currentLayout);
		vkCmdCopyImageToBuffer(m_CommandBuffer, image.GetAs<VulkanImage>()->GetImage(), vkLayout, buffer.GetAs<VulkanBuffer>()->GetBuffer(), regionCount, copies);
		StackFreeLast();
	}

	void VulkanCommandList::SetViewport(const Viewport* viewports, uint viewportCount)
	{
		VkViewport* vkViewports = StackAlloc<VkViewport>(viewportCount);
		// Flip y as Vulkan has an inverted clip space compared to DirectX
		for (uint i = 0; i < viewportCount; ++i)
		{
			vkViewports[i].x = viewports[i].x;
			// y offset needs to add the height as part of inverting y
			vkViewports[i].y = viewports[i].y + viewports[i].height;
			vkViewports[i].width = viewports[i].width;
			vkViewports[i].height = -viewports[i].height;
			vkViewports[i].minDepth = viewports[i].minDepth;
			vkViewports[i].maxDepth = viewports[i].maxDepth;
		}
		vkCmdSetViewport(m_CommandBuffer, 0, viewportCount, vkViewports);
		StackFreeLast();
	}

	void VulkanCommandList::SetScissor(const GraphicsRect* scissors, uint scissorCount)
	{
		VkRect2D* vkScissors = StackAlloc<VkRect2D>(scissorCount);
		for (uint i = 0; i < scissorCount; ++i)
		{
			vkScissors[i].offset.x = scissors[i].offset.x;
			vkScissors[i].offset.y = scissors[i].offset.y;
			vkScissors[i].extent.width = scissors[i].extents.width;
			vkScissors[i].extent.height = scissors[i].extents.height;
		}
		vkCmdSetScissor(m_CommandBuffer, 0, scissorCount, vkScissors);
		StackFreeLast();
	}

	void VulkanCommandList::BindPipeline(const Ref<Pipeline>& pipeline)
	{
		VkPipelineBindPoint bindPoint; 
		VkPipeline vkPipeline;
		switch (pipeline->GetPipelineType())
		{
		case PipelineType::Graphics:
			bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			vkPipeline = RefCast<VulkanGraphicsPipeline>(pipeline)->GetPipeline();
			break;
		case PipelineType::Compute:
			bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
			vkPipeline = RefCast<VulkanComputePipeline>(pipeline)->GetPipeline();
			break;
		case  PipelineType::RayTracing:
			bindPoint = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
			vkPipeline = RefCast<VulkanRayTracingPipeline>(pipeline)->GetPipeline();
			break;
		default:
			TYR_ASSERT(false);
			return;
		}
		vkCmdBindPipeline(m_CommandBuffer, bindPoint, vkPipeline);
	}

	void VulkanCommandList::BindVertexBuffers(const Buffer* const* vertexBuffers, uint bufferCount)
	{
		TYR_ASSERT(bufferCount > 0);

		StackAllocManager stack;
		VkBuffer* buffers = stack.Alloc<VkBuffer>(bufferCount);
		VkDeviceSize* offsets = stack.Alloc<VkDeviceSize>(bufferCount);

		for (uint i = 0; i < bufferCount; ++i)
		{
			const VulkanBuffer* buffer = static_cast<const VulkanBuffer*>(vertexBuffers[i]);
			buffers[i] = buffer->GetBuffer();
			offsets[i] = 0;
		}
		vkCmdBindVertexBuffers(m_CommandBuffer, 0, bufferCount, buffers, offsets);
	}

	void VulkanCommandList::BindVertexBuffers(const BufferView* const* vertexBufferViews, uint bufferViewCount)
	{
		TYR_ASSERT(bufferViewCount > 0);

		StackAllocManager stack;
		VkBuffer* buffers = stack.Alloc<VkBuffer>(bufferViewCount);
		VkDeviceSize* offsets = stack.Alloc<VkDeviceSize>(bufferViewCount);

		for (uint i = 0; i < bufferViewCount; ++i)
		{
			const VulkanBufferView* bufferView = static_cast<const VulkanBufferView*>(vertexBufferViews[i]);
			const BufferViewDesc& viewDesc = bufferView->GetDesc();
			buffers[i] = bufferView->GetBuffer()->GetBuffer();
			offsets[i] = viewDesc.offset;
		}
		vkCmdBindVertexBuffers(m_CommandBuffer, 0, bufferViewCount, buffers, offsets);
	}

	void VulkanCommandList::BindIndexBuffer(const SRef<Buffer>& indexBuffer, size_t offset)
	{
		const VulkanBuffer* buffer = indexBuffer.GetAs<VulkanBuffer>();
		BindIndexBufferInternal(buffer, offset);
	}

	void VulkanCommandList::BindIndexBuffer(const SRef<BufferView>& indexBufferView)
	{
		const VulkanBufferView* vulkanBufferView = indexBufferView.GetAs<VulkanBufferView>();
		const BufferViewDesc& viewDesc = indexBufferView->GetDesc();
		BindIndexBufferInternal(vulkanBufferView->GetBuffer(), viewDesc.offset);
	}

	void VulkanCommandList::BindIndexBufferInternal(const VulkanBuffer* buffer, size_t offset)
	{
		const BufferDesc& bufferDesc = buffer->GetDesc();
		VkIndexType type;
		if (bufferDesc.stride == 2)
		{
			type = VK_INDEX_TYPE_UINT16;
		}
		else if (bufferDesc.stride == 4)
		{
			type = VK_INDEX_TYPE_UINT32;
		}
		else
		{
			type = VK_INDEX_TYPE_UINT16;
			TYR_ASSERT(false);
		}

		vkCmdBindIndexBuffer(m_CommandBuffer, buffer->GetBuffer(), static_cast<VkDeviceSize>(offset), type);
	}

	void VulkanCommandList::BindDescriptorSetGroup(const Ref<DescriptorSetGroup>& group, const Ref<Pipeline>& pipeline)
	{
		const DescriptorSetGroupDesc& groupDesc = group->GetDesc();
		const Ref<VulkanDescriptorSetGroup>& vulkanGroup = RefCast<VulkanDescriptorSetGroup>(group);

		PipelineType pipelineType = pipeline->GetPipelineType();
		VkPipelineLayout vkPipelineLayout;
		VkPipelineBindPoint vkPipelineBindPoint;
		switch (pipelineType)
		{
		case PipelineType::Graphics:
			vkPipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			vkPipelineLayout = RefCast<VulkanGraphicsPipeline>(pipeline)->GetPipelineLayout();
			break;
		case PipelineType::Compute:
			vkPipelineBindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
			vkPipelineLayout = RefCast<VulkanComputePipeline>(pipeline)->GetPipelineLayout();
			break;
		case  PipelineType::RayTracing:
			vkPipelineBindPoint = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
			vkPipelineLayout = RefCast<VulkanRayTracingPipeline>(pipeline)->GetPipelineLayout();
			break;
		default:
			TYR_ASSERT(false);
			return;
		}
		const VkDescriptorSet* sets = vulkanGroup->GetSets();
		vkCmdBindDescriptorSets(m_CommandBuffer, vkPipelineBindPoint, vkPipelineLayout, 0, groupDesc.layoutCount,
			sets, 0, nullptr);
	}

	void VulkanCommandList::DrawIndexed(uint indexCount, uint instanceCount, uint firstIndex, int vertexOffset, uint firstInstance)
	{
		vkCmdDrawIndexed(m_CommandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
	}

	void VulkanCommandList::Draw(uint vertexCount, uint instanceCount, uint firstVertex, uint firstInstance)
	{
		vkCmdDraw(m_CommandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
	}

	void VulkanCommandList::DrawMeshTasks(uint groupCountX, uint groupCountY, uint groupCountZ)
	{
		// TODO: Add the extension
		vkCmdDrawMeshTasksEXT(m_CommandBuffer, groupCountX, groupCountY, groupCountZ);
	}

	void VulkanCommandList::Dispatch(uint groupCountX, uint groupCountY, uint groupCountZ)
	{
		vkCmdDispatch(m_CommandBuffer, groupCountX, groupCountY, groupCountZ);
	}

	void VulkanCommandList::Execute(const CommndListExecuteDesc* executeDescs, uint executeDescCount, uint queueIndex, const Ref<Fence>& fence)
	{
		TYR_ASSERT(executeDescs && executeDescCount > 0);
		StackAllocManager stack;
		VkSubmitInfo* submitInfos = stack.Alloc<VkSubmitInfo>(executeDescCount);
		for (uint i = 0; i < executeDescCount; ++i)
		{
			const CommndListExecuteDesc& executeDesc = executeDescs[i];
			TYR_ASSERT(executeDesc.commandLists.Size() > 0);

			// The command lists should include this one.
			VkCommandBuffer* commandBuffers = stack.Alloc<VkCommandBuffer>(executeDesc.commandLists.Size());
			for (size_t j = 0; j < executeDesc.commandLists.Size(); ++j)
			{
				const Ref<VulkanCommandList>& commandList = RefCast<VulkanCommandList>(executeDesc.commandLists[j]);
				commandBuffers[j] = commandList->m_CommandBuffer;
			}

			VkSemaphore* waitSemaphores = nullptr;
			if (executeDesc.waitSemaphores.Size() > 0)
			{
				waitSemaphores = stack.Alloc<VkSemaphore>(executeDesc.waitSemaphores.Size());
				for (size_t j = 0; j < executeDesc.waitSemaphores.Size(); ++j)
				{
					const Ref<VulkanSemaphore>& semaphore = RefCast<VulkanSemaphore>(executeDesc.waitSemaphores[j]);
					waitSemaphores[j] = semaphore->GetSemaphore();
				}
			}

			VkPipelineStageFlags* waitDstPipelineStages = nullptr;
			if (executeDesc.waitDstPipelineStages.Size() > 0)
			{
				waitDstPipelineStages = stack.Alloc<VkPipelineStageFlags>(executeDesc.waitDstPipelineStages.Size());
				for (size_t j = 0; j < executeDesc.waitDstPipelineStages.Size(); ++j)
				{
					waitDstPipelineStages[j] = static_cast<VkPipelineStageFlags>(executeDesc.waitDstPipelineStages[j]);
				}
			}

			VkSemaphore* signalSemaphores = nullptr;
			if (executeDesc.signalSemaphores.Size() > 0)
			{
				signalSemaphores = stack.Alloc<VkSemaphore>(executeDesc.signalSemaphores.Size());
				for (size_t j = 0; j < executeDesc.signalSemaphores.Size(); ++j)
				{
					const Ref<VulkanSemaphore>& semaphore = RefCast<VulkanSemaphore>(executeDesc.signalSemaphores[j]);
					signalSemaphores[j] = semaphore->GetSemaphore();
				}
			}

			VkTimelineSemaphoreSubmitInfo* timelineSemaphoreSubmitInfo = nullptr;
			if (!executeDesc.waitValues.IsEmpty() || !executeDesc.signalValues.IsEmpty())
			{
				timelineSemaphoreSubmitInfo = stack.Alloc<VkTimelineSemaphoreSubmitInfo>();
				timelineSemaphoreSubmitInfo->sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
				timelineSemaphoreSubmitInfo->pNext = nullptr;
				timelineSemaphoreSubmitInfo->waitSemaphoreValueCount = !executeDesc.waitValues.IsEmpty() ? static_cast<uint>(executeDesc.waitValues.Size()) : 0;
				timelineSemaphoreSubmitInfo->pWaitSemaphoreValues = !executeDesc.waitValues.IsEmpty() ? executeDesc.waitValues.Data() : nullptr;
				timelineSemaphoreSubmitInfo->signalSemaphoreValueCount = !executeDesc.signalValues.IsEmpty() ? static_cast<uint>(executeDesc.signalValues.Size()) : 0;
				timelineSemaphoreSubmitInfo->pSignalSemaphoreValues = !executeDesc.signalValues.IsEmpty() ? executeDesc.signalValues.Data() : nullptr;
			}

			submitInfos[i].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfos[i].pNext = timelineSemaphoreSubmitInfo;
			submitInfos[i].waitSemaphoreCount = static_cast<uint>(executeDesc.waitSemaphores.Size());
			submitInfos[i].pWaitSemaphores = waitSemaphores;
			submitInfos[i].pWaitDstStageMask = waitDstPipelineStages;
			submitInfos[i].commandBufferCount = static_cast<uint>(executeDesc.commandLists.Size());
			submitInfos[i].pCommandBuffers = commandBuffers;
			submitInfos[i].signalSemaphoreCount = static_cast<uint>(executeDesc.signalSemaphores.Size());
			submitInfos[i].pSignalSemaphores = signalSemaphores;
		}

		VkFence vkFence = fence ? RefCast<VulkanFence>(fence)->GetFence() : VK_NULL_HANDLE;
		VkQueue queue = m_Device.GetQueue(m_QueueType, queueIndex);
		TYR_GASSERT(vkQueueSubmit(queue, executeDescCount, submitInfos, vkFence));
	}
}