#include "VulkanCommandList.h"
#include "Memory/StackAllocation.h"
#include "VulkanCommandAllocator.h"
#include "VulkanSync.h"
#include "VulkanBuffer.h"
#include "VulkanImage.h"
#include "VulkanDescriptorSetGroup.h"
#include "VulkanPipeline.h"
#include "VulkanExtensions.h"

namespace tyr
{
	CommandListInternal::CommandListInternal(DeviceInternal& device, const CommandListDesc& desc)
		: CommandList(desc)
		, m_Device(device)
	{
		m_CommandPool = static_cast<VulkanCommandAllocator*>(desc.allocator)->GetCommandPool();
		m_QueueType = m_Desc.allocator->GetDesc().queueType;
		m_QueueFamilyIndex = device.GetQueueFamilyIndex(m_QueueType);
		VkCommandBufferAllocateInfo info;
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		info.pNext = nullptr;
		info.commandPool = m_CommandPool;
		info.level = VulkanUtility::ToVulkanCommandBufferLevel(desc.type);
		info.commandBufferCount = 1;

		vkAllocateCommandBuffers(device.GetLogicalDevice(), &info, &m_CommandBuffer);
#if !TYR_FINAL
		VulkanUtility::SetDebugName(device.GetLogicalDevice(), desc.debugName.CStr(), VK_OBJECT_TYPE_COMMAND_BUFFER, reinterpret_cast<uint64>(m_CommandBuffer));
#endif
	}

	CommandListInternal::~CommandListInternal()
	{
		vkFreeCommandBuffers(m_Device.GetLogicalDevice(), m_CommandPool, 1, &m_CommandBuffer);
	}
	
	void CommandList::Begin(CommandBufferUsage usage)
	{
		CommandListInternal& commandList = static_cast<CommandListInternal&>(*this);

		VkCommandBufferBeginInfo info;
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		info.pNext = nullptr;
		info.flags = static_cast<VkCommandBufferUsageFlags>(usage);
		info.pInheritanceInfo = nullptr;

		TYR_GASSERT(vkBeginCommandBuffer(commandList.m_CommandBuffer, &info));
	}

	void CommandList::End()
	{
		CommandListInternal& commandList = static_cast<CommandListInternal&>(*this);
		TYR_GASSERT(vkEndCommandBuffer(commandList.m_CommandBuffer));
	}

	void CommandList::Reset(bool releaseResources)
	{
		CommandListInternal& commandList = static_cast<CommandListInternal&>(*this);
		TYR_GASSERT(vkResetCommandBuffer(commandList.m_CommandBuffer, releaseResources ? VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT : 0));
	}

	void CommandList::ClearColourImage(ImageHandle image, ImageLayout layout, const ClearColourValue& clearValue, const SubresourceRange* subresourceRanges, uint rangeCount)
	{
		CommandListInternal& commandList = static_cast<CommandListInternal&>(*this);
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
		Image imageData = commandList.m_Device.GetImage(image);
		vkCmdClearColorImage(commandList.m_CommandBuffer, imageData.image, static_cast<VkImageLayout>(layout), vkClearValue, rangeCount, vkSubresourceRanges);
		StackFreeLast();
	}

	void CommandList::ClearDepthStencilImage(ImageHandle image, ImageLayout layout, const ClearDepthStencilValue& clearValue, const SubresourceRange* subresourceRanges, uint rangeCount)
	{
		CommandListInternal& commandList = static_cast<CommandListInternal&>(*this);
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
		Image imageData = commandList.m_Device.GetImage(image);
		vkCmdClearDepthStencilImage(commandList.m_CommandBuffer, imageData.image, static_cast<VkImageLayout>(layout), vkClearValue, rangeCount, vkSubresourceRanges);
		StackFreeLast();
	}

	void CommandList::BeginRendering(const RenderingInfo& renderingInfo) 
	{
		CommandListInternal& commandList = static_cast<CommandListInternal&>(*this);
		static auto CreateVulkankRenderingAttachmentInfo = [&commandList](const RenderingAttachmentInfo& renderingAttachmentInfo,
			VkRenderingAttachmentInfo& vkRenderingAttachmentInfo) -> void
		{
			vkRenderingAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			vkRenderingAttachmentInfo.pNext = nullptr;
			vkRenderingAttachmentInfo.imageView = commandList.m_Device.GetImageView(renderingAttachmentInfo.imageView).imageView;
			vkRenderingAttachmentInfo.imageLayout = VulkanUtility::ToVulkanImageLayout(renderingAttachmentInfo.imageLayout);
			vkRenderingAttachmentInfo.resolveMode = static_cast<VkResolveModeFlagBits>(renderingAttachmentInfo.resolveMode);
			if (renderingAttachmentInfo.resolveImageView)
			{
				vkRenderingAttachmentInfo.resolveImageView = commandList.m_Device.GetImageView(renderingAttachmentInfo.resolveImageView).imageView;
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
		if (renderingInfo.hasDepthAttachment)
		{
			vkDepthAttachment = stack.Alloc<VkRenderingAttachmentInfo>();
			CreateVulkankRenderingAttachmentInfo(renderingInfo.depthAttachment, *vkDepthAttachment);
		}

		VkRenderingAttachmentInfo* vkStencilAttachment = nullptr;
		if (renderingInfo.hasDepthAttachment)
		{
			vkStencilAttachment = stack.Alloc<VkRenderingAttachmentInfo>();
			CreateVulkankRenderingAttachmentInfo(renderingInfo.stencilAttachment, *vkStencilAttachment);
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

		vkCmdBeginRendering(commandList.m_CommandBuffer, &vkRenderingInfo);
	}

	void CommandList::EndRendering() 
	{
		CommandListInternal& commandList = static_cast<CommandListInternal&>(*this);
		vkCmdEndRendering(commandList.m_CommandBuffer);
	}

	void CommandList::AddBarriers(const BufferBarrier* bufferBarriers, uint bufferBarrierCount,
		const ImageBarrier* imageBarriers, uint imageBarrierCount,
		const PipelineBarrier* pipelineBarriers, uint pipelineBarrierCount,
		Dependency dependency)
	{
		CommandListInternal& commandList = static_cast<CommandListInternal&>(*this);
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
				Buffer& buffer = commandList.m_Device.GetBuffer(bufferBarriers[i].buffer);
				vkBufferBarriers[i].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
				vkBufferBarriers[i].pNext = nullptr;
				vkBufferBarriers[i].srcAccessMask = static_cast<VkAccessFlags2>(bufferBarriers[i].srcAccess);
				vkBufferBarriers[i].dstAccessMask = static_cast<VkAccessFlags2>(bufferBarriers[i].dstAccess);
				vkBufferBarriers[i].buffer = buffer.buffer;
				vkBufferBarriers[i].offset = bufferBarriers[i].offset;
				vkBufferBarriers[i].size = bufferBarriers[i].size;
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
				Image& image = commandList.m_Device.GetImage(imageBarriers[i].image);
				vkImageBarriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
				vkImageBarriers[i].pNext = nullptr;
				vkImageBarriers[i].srcAccessMask = static_cast<VkAccessFlags2>(imageBarriers[i].srcAccess);
				vkImageBarriers[i].dstAccessMask = static_cast<VkAccessFlags2>(imageBarriers[i].dstAccess);
				vkImageBarriers[i].image = image.image;
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
		vkDependencyInfo.memoryBarrierCount = static_cast<uint>(pipelineBarrierCount);
		vkDependencyInfo.pMemoryBarriers = vkMemoryBarriers;
		vkDependencyInfo.bufferMemoryBarrierCount = static_cast<uint>(bufferBarrierCount);
		vkDependencyInfo.pBufferMemoryBarriers = vkBufferBarriers;
		vkDependencyInfo.imageMemoryBarrierCount = static_cast<uint>(imageBarrierCount);
		vkDependencyInfo.pImageMemoryBarriers = vkImageBarriers;

		vkCmdPipelineBarrier2(commandList.m_CommandBuffer, &vkDependencyInfo);
	}

	void CommandList::CopyBuffer(BufferHandle srcBuffer, BufferHandle dstBuffer, const BufferCopyInfo* copyInfos, uint copyInfoCount)
	{
		CommandListInternal& commandList = static_cast<CommandListInternal&>(*this);
		Buffer& src = commandList.m_Device.GetBuffer(srcBuffer);
		Buffer& dst = commandList.m_Device.GetBuffer(dstBuffer);
		VkBufferCopy* copies = StackAlloc<VkBufferCopy>(copyInfoCount);
		for (uint i = 0; i < copyInfoCount; ++i)
		{
			copies[i].srcOffset = static_cast<VkDeviceSize>(copyInfos[i].srcOffset);
			copies[i].dstOffset = static_cast<VkDeviceSize>(copyInfos[i].dstOffset);
			copies[i].size = static_cast<VkDeviceSize>(copyInfos[i].size);
		}
		vkCmdCopyBuffer(commandList.m_CommandBuffer, src.buffer, dst.buffer, copyInfoCount, copies);
		StackFreeLast();
	}

	void CommandList::CopyBuffer(BufferHandle srcBuffer, BufferHandle dstBuffer)
	{
		CommandListInternal& commandList = static_cast<CommandListInternal&>(*this);
		Buffer& src = commandList.m_Device.GetBuffer(srcBuffer);
		Buffer& dst = commandList.m_Device.GetBuffer(dstBuffer);
		VkBufferCopy vkBufferCopy;
		vkBufferCopy.srcOffset = 0;
		vkBufferCopy.dstOffset = 0;
		vkBufferCopy.size = src.size;
		vkCmdCopyBuffer(commandList.m_CommandBuffer, src.buffer, dst.buffer, 1, &vkBufferCopy);
	}

	void CommandList::CopyBufferToImage(BufferHandle buffer, ImageHandle image, ImageLayout targetLayout, const BufferImageCopyInfo* regions, uint regionCount)
	{
		CommandListInternal& commandList = static_cast<CommandListInternal&>(*this);
		Buffer& src = commandList.m_Device.GetBuffer(buffer);
		Image& dst = commandList.m_Device.GetImage(image);

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
		vkCmdCopyBufferToImage(commandList.m_CommandBuffer, src.buffer, dst.image, vkLayout, regionCount, copies);
		StackFreeLast();
	}

	void CommandList::CopyImage(ImageHandle srcImage, ImageLayout srcLayout, ImageHandle dstImage, ImageLayout dstLayout, const ImageCopyInfo* regions, uint regionCount)
	{
		CommandListInternal& commandList = static_cast<CommandListInternal&>(*this);
		Image& src = commandList.m_Device.GetImage(srcImage);
		Image& dst = commandList.m_Device.GetImage(dstImage);
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
		vkCmdCopyImage(commandList.m_CommandBuffer, src.image, vkSrcLayout, dst.image, vkDstLayout, regionCount, copies);
		StackFreeLast();
	}

	void CommandList::CopyImageToBuffer(ImageHandle image, BufferHandle buffer, ImageLayout currentLayout, const BufferImageCopyInfo* regions, uint regionCount)
	{
		CommandListInternal& commandList = static_cast<CommandListInternal&>(*this);
		Image& src = commandList.m_Device.GetImage(image);
		Buffer& dst = commandList.m_Device.GetBuffer(buffer);
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
		vkCmdCopyImageToBuffer(commandList.m_CommandBuffer, src.image, vkLayout, dst.buffer, regionCount, copies);
		StackFreeLast();
	}

	void CommandList::SetViewport(const Viewport* viewports, uint viewportCount)
	{
		CommandListInternal& commandList = static_cast<CommandListInternal&>(*this);
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
		vkCmdSetViewport(commandList.m_CommandBuffer, 0, viewportCount, vkViewports);
		StackFreeLast();
	}

	void CommandList::SetScissor(const GraphicsRect* scissors, uint scissorCount)
	{
		CommandListInternal& commandList = static_cast<CommandListInternal&>(*this);
		VkRect2D* vkScissors = StackAlloc<VkRect2D>(scissorCount);
		for (uint i = 0; i < scissorCount; ++i)
		{
			vkScissors[i].offset.x = scissors[i].offset.x;
			vkScissors[i].offset.y = scissors[i].offset.y;
			vkScissors[i].extent.width = scissors[i].extents.width;
			vkScissors[i].extent.height = scissors[i].extents.height;
		}
		vkCmdSetScissor(commandList.m_CommandBuffer, 0, scissorCount, vkScissors);
		StackFreeLast();
	}

	void CommandList::BindGraphicsPipeline(GraphicsPipelineHandle pipeline)
	{
		CommandListInternal& commandList = static_cast<CommandListInternal&>(*this);
		vkCmdBindPipeline(commandList.m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, commandList.m_Device.GetGraphicsPipeline(pipeline).pipeline);
	}

	void CommandList::BindComputePipeline(ComputePipelineHandle pipeline)
	{
		CommandListInternal& commandList = static_cast<CommandListInternal&>(*this);
		vkCmdBindPipeline(commandList.m_CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, commandList.m_Device.GetComputePipeline(pipeline).pipeline);
	}

	void CommandList::BindRayTracingPipeline(RayTracingPipelineHandle pipeline)
	{
		CommandListInternal& commandList = static_cast<CommandListInternal&>(*this);
		vkCmdBindPipeline(commandList.m_CommandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, commandList.m_Device.GetRayTracingPipeline(pipeline).pipeline);
	}

	void CommandList::BindVertexBuffers(const BufferHandle* vertexBuffers, uint bufferCount)
	{
		TYR_ASSERT(bufferCount > 0);

		CommandListInternal& commandList = static_cast<CommandListInternal&>(*this);

		StackAllocManager stack;
		VkBuffer* buffers = stack.Alloc<VkBuffer>(bufferCount);
		VkDeviceSize* offsets = stack.Alloc<VkDeviceSize>(bufferCount);

		for (uint i = 0; i < bufferCount; ++i)
		{
			buffers[i] = commandList.m_Device.GetBuffer(vertexBuffers[i]).buffer;
			offsets[i] = 0;
		}
		vkCmdBindVertexBuffers(commandList.m_CommandBuffer, 0, bufferCount, buffers, offsets);
	}

	void CommandList::BindVertexBuffers(const BufferViewHandle* vertexBufferViews, uint bufferViewCount)
	{
		TYR_ASSERT(bufferViewCount > 0);

		CommandListInternal& commandList = static_cast<CommandListInternal&>(*this);

		StackAllocManager stack;
		VkBuffer* buffers = stack.Alloc<VkBuffer>(bufferViewCount);
		VkDeviceSize* offsets = stack.Alloc<VkDeviceSize>(bufferViewCount);

		for (uint i = 0; i < bufferViewCount; ++i)
		{
			const BufferView& view = commandList.m_Device.GetBufferView(vertexBufferViews[i]);
			buffers[i] = commandList.m_Device.GetBuffer(view.buffer).buffer;
			offsets[i] = view.offset;
		}
		vkCmdBindVertexBuffers(commandList.m_CommandBuffer, 0, bufferViewCount, buffers, offsets);
	}

	void CommandList::BindIndexBuffer(BufferHandle indexBuffer, size_t offset)
	{
		CommandListInternal& commandList = static_cast<CommandListInternal&>(*this);
		commandList.BindIndexBufferInternal(indexBuffer, offset);
	}

	void CommandList::BindIndexBuffer(BufferViewHandle indexBufferView)
	{
		CommandListInternal& commandList = static_cast<CommandListInternal&>(*this);
		const BufferView& view = commandList.m_Device.GetBufferView(indexBufferView);
		commandList.BindIndexBufferInternal(view.buffer, view.offset);
	}

	void CommandListInternal::BindIndexBufferInternal(BufferHandle buffer, size_t offset)
	{
		Buffer& bufferData = m_Device.GetBuffer(buffer);

		VkIndexType type;
		if (bufferData.stride == 2)
		{
			type = VK_INDEX_TYPE_UINT16;
		}
		else if (bufferData.stride == 4)
		{
			type = VK_INDEX_TYPE_UINT32;
		}
		else
		{
			type = VK_INDEX_TYPE_UINT16;
			TYR_ASSERT(false);
		}

		vkCmdBindIndexBuffer(m_CommandBuffer, bufferData.buffer, static_cast<VkDeviceSize>(offset), type);
	}

	void CommandList::BindDescriptorSet(DescriptorSetGroupHandle group, GraphicsPipelineHandle pipeline)
	{
		CommandListInternal& commandList = static_cast<CommandListInternal&>(*this);

		const DescriptorSetGroup& groupData = commandList.m_Device.GetDescriptorSetGroup(group);
		const GraphicsPipeline& pipelineData = commandList.m_Device.GetGraphicsPipeline(pipeline);

		vkCmdBindDescriptorSets(commandList.m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineData.pipelineLayout, 0, groupData.layouts.Size(),
			groupData.sets.Data(), 0, nullptr);
	}

	void CommandList::BindDescriptorSet(DescriptorSetGroupHandle group, ComputePipelineHandle pipeline)
	{
		CommandListInternal& commandList = static_cast<CommandListInternal&>(*this);

		const DescriptorSetGroup& groupData = commandList.m_Device.GetDescriptorSetGroup(group);
		const ComputePipeline& pipelineData = commandList.m_Device.GetComputePipeline(pipeline);

		vkCmdBindDescriptorSets(commandList.m_CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.pipelineLayout, 0, groupData.layouts.Size(),
			groupData.sets.Data(), 0, nullptr);
	}

	void CommandList::BindDescriptorSet(DescriptorSetGroupHandle group, RayTracingPipelineHandle pipeline)
	{
		CommandListInternal& commandList = static_cast<CommandListInternal&>(*this);

		const DescriptorSetGroup& groupData = commandList.m_Device.GetDescriptorSetGroup(group);
		const RayTracingPipeline& pipelineData = commandList.m_Device.GetRayTracingPipeline(pipeline);

		vkCmdBindDescriptorSets(commandList.m_CommandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineData.pipelineLayout, 0, groupData.layouts.Size(),
			groupData.sets.Data(), 0, nullptr);
	}

	void CommandList::DrawIndexed(uint indexCount, uint instanceCount, uint firstIndex, int vertexOffset, uint firstInstance)
	{
		CommandListInternal& commandList = static_cast<CommandListInternal&>(*this);
		vkCmdDrawIndexed(commandList.m_CommandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
	}

	void CommandList::Draw(uint vertexCount, uint instanceCount, uint firstVertex, uint firstInstance)
	{
		CommandListInternal& commandList = static_cast<CommandListInternal&>(*this);
		vkCmdDraw(commandList.m_CommandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
	}

	void CommandList::DrawMeshTasks(uint groupCountX, uint groupCountY, uint groupCountZ)
	{
		CommandListInternal& commandList = static_cast<CommandListInternal&>(*this);
		// TODO: Add the extension
		vkCmdDrawMeshTasksEXT(commandList.m_CommandBuffer, groupCountX, groupCountY, groupCountZ);
	}

	void CommandList::Dispatch(uint groupCountX, uint groupCountY, uint groupCountZ)
	{
		CommandListInternal& commandList = static_cast<CommandListInternal&>(*this);
		vkCmdDispatch(commandList.m_CommandBuffer, groupCountX, groupCountY, groupCountZ);
	}

	void CommandList::Execute(const CommndListExecuteDesc* executeDescs, uint executeDescCount, uint queueIndex, FenceHandle fence)
	{
		TYR_ASSERT(executeDescs && executeDescCount > 0);

		CommandListInternal& commandList = static_cast<CommandListInternal&>(*this);

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
				const CommandListInternal* cmdList = static_cast<CommandListInternal*>(executeDesc.commandLists[j]);
				commandBuffers[j] = cmdList->m_CommandBuffer;
			}

			VkSemaphore* waitSemaphores = nullptr;
			if (executeDesc.waitSemaphores.Size() > 0)
			{
				waitSemaphores = stack.Alloc<VkSemaphore>(executeDesc.waitSemaphores.Size());
				for (size_t j = 0; j < executeDesc.waitSemaphores.Size(); ++j)
				{
					const Semaphore& semaphore = commandList.m_Device.GetSemaphore(executeDesc.waitSemaphores[j]);
					waitSemaphores[j] = semaphore.semaphore;
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
					const Semaphore& semaphore = commandList.m_Device.GetSemaphore(executeDesc.signalSemaphores[j]);
					signalSemaphores[j] = semaphore.semaphore;
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

		VkFence vkFence = fence ? commandList.m_Device.GetFence(fence).fence : VK_NULL_HANDLE;
		VkQueue queue = commandList.m_Device.GetQueue(m_QueueType, queueIndex);
		TYR_GASSERT(vkQueueSubmit(queue, executeDescCount, submitInfos, vkFence));
	}
}