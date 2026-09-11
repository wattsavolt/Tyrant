
#pragma once

#include "GraphicsBase.h"
#include "RenderAPITypes.h"
#include "Image.h"
#include "Geometry/GeometryTypes.h"
#include "Device.h"

namespace tyr
{
	struct RenderingAttachmentInfo
	{
		ClearValue clearValue;
		ImageViewHandle imageView;
		ImageViewHandle resolveImageView;
		ResolveMode resolveMode;
		ImageLayout imageLayout;
		ImageLayout resolveImageLayout = ImageLayout::IMAGE_LAYOUT_UNKNOWN;
		AttachmentLoadOp loadOp;
		AttachmentStoreOp storeOp;
	};

	struct RenderingInfo
	{
		GraphicsRect renderArea;
		RenderingInfoFlags flags = RENDERING_INFO_NONE;
		uint layerCount;
		uint viewMask;
		uint colourAttachmentCount = 0;
		RenderingAttachmentInfo* colourAttachments;
		RenderingAttachmentInfo depthAttachment;
		RenderingAttachmentInfo stencilAttachment;
		bool hasDepthAttachment = false;
		bool hasStencilAttachment = false;
	};

	struct CommandListDesc
	{
		GDebugString debugName;
		CommandListType type = CommandListType::Primary;
		CommandAllocator* allocator;
	};

	/// Class repesenting a command buffer 
	class TYR_GRAPHICS_API CommandList
	{
	public:
		CommandList(const CommandListDesc& desc);
		virtual ~CommandList() = default;
		void Begin(CommandBufferUsage usage);
		void End();
		void Reset(bool releaseResources);
		void ClearColourImage(ImageHandle image, ImageLayout layout, const ClearColourValue& clearValue, const SubresourceRange* subresourceRanges, uint rangeCount);
		void ClearDepthStencilImage(ImageHandle image, ImageLayout layout, const ClearDepthStencilValue& clearValue, const SubresourceRange* subresourceRanges, uint rangeCount);
		void BeginRendering(const RenderingInfo& renderingInfo);
		void EndRendering();
		// TODO: If phones are supported, add wrapper functions for vkCmdBeginRenderPass and vkCmdEndRenderPass for render passes
		void AddBarriers(const BufferBarrier* bufferBarriers, uint bufferBarrierCount,
			const ImageBarrier* imageBarriers = nullptr, uint imageBarrierCounts = 0,
			const PipelineBarrier* pipelineBarriers = nullptr, uint pipelineBarrierCounts = 0,
			Dependency dependency = DEPENDENCY_UNKNOWN);
		void CopyBuffer(BufferHandle srcBuffer, BufferHandle dstBuffer, const BufferCopyInfo* copyInfos, uint copyInfoCount);
		void CopyBuffer(BufferHandle srcBuffer, BufferHandle dstBuffer);
		void CopyBufferToImage(BufferHandle buffer, ImageHandle image, ImageLayout targetLayout, const BufferImageCopyInfo* regions, uint regionCount);
		void CopyImage(ImageHandle srcImage, ImageLayout srcLayout, ImageHandle dstImage, ImageLayout dstLayout, const ImageCopyInfo* regions, uint regionCount);
		void CopyImageToBuffer(ImageHandle image, BufferHandle buffer, ImageLayout currentLayout, const BufferImageCopyInfo* regions, uint regionCount);
		void SetViewport(const Viewport* viewports, uint viewportCount);
		void SetScissor(const GraphicsRect* scissors, uint scissorCount);
		void BindGraphicsPipeline(GraphicsPipelineHandle pipeline);
		void BindComputePipeline(ComputePipelineHandle pipeline);
		void BindRayTracingPipeline(RayTracingPipelineHandle pipeline);
		// Caller must ensure lifetime of buffers passed exceeds the function call
		void BindVertexBuffers(const BufferHandle* vertexBuffers, uint bufferCount);
		// Caller must ensure lifetime of buffer views passed exceeds the function call
		void BindVertexBuffers(const BufferViewHandle* vertexBufferViews, uint bufferViewCount);
		void BindIndexBuffer(BufferHandle indexBuffer, size_t offset);
		void BindIndexBuffer(BufferViewHandle indexBufferView);
		void BindDescriptorSet(DescriptorSetHandle set, GraphicsPipelineHandle pipeline);
		void BindDescriptorSet(DescriptorSetHandle set, ComputePipelineHandle pipeline);
		void BindDescriptorSet(DescriptorSetHandle set, RayTracingPipelineHandle pipeline);
		void DrawIndexed(uint indexCount, uint instanceCount, uint firstIndex, int vertexOffset, uint firstInstance);
		void Draw(uint vertexCount, uint instanceCount, uint firstVertex, uint firstInstance);
		void DrawMeshTasks(uint groupCountX, uint groupCountY, uint groupCountZ);
		void Dispatch(uint groupCountX, uint groupCountY, uint groupCountZ);

		const CommandListDesc& GetDesc() const { return m_Desc; }

	protected:
		CommandListDesc m_Desc;
	};
}
