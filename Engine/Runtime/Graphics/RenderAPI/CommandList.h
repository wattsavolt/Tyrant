
#pragma once

#include "GraphicsBase.h"
#include "RenderAPITypes.h"
#include "Image.h"
#include "Geometry/GeometryTypes.h"

namespace tyr
{
	class CommandAllocator;
	class CommandList;
	class Fence;
	class Semaphore;
	class Event;
	class Buffer;
	class Buffer;
	class BufferView;
	class Pipeline;
	class DescriptorSetGroup;
	class ImageView;
	struct BufferCopyInfo;
	struct BufferImageCopyInfo;
	struct ImageCopyInfo;
	struct PipelineBarrier;
	struct BufferBarrier;
	struct ImageBarrier;
	struct Viewport;
	struct GraphicsRect;

	struct RenderingAttachmentInfo
	{
		Ref<ImageView> imageView;
		ImageLayout imageLayout;
		Ref<ImageView> resolveImageView;
		ImageLayout resolveImageLayout = ImageLayout::IMAGE_LAYOUT_UNKNOWN;
		ResolveMode resolveMode;
		AttachmentLoadOp loadOp;
		AttachmentStoreOp storeOp;
		ClearValue clearValue;
	};

	struct RenderingInfo
	{
		RenderingInfoFlags flags = RENDERING_INFO_NONE;
		GraphicsRect renderArea;
		uint layerCount;
		uint viewMask;
		Array<RenderingAttachmentInfo>	colourAttachments;
		// Optional
		URef<RenderingAttachmentInfo> depthAttachment;
		// Optional
		URef<RenderingAttachmentInfo> stencilAttachment;
	};

	struct CommandListDesc
	{
		String debugName; 
		CommandListType type = CommandListType::Primary;
		Ref<CommandAllocator> allocator;
	};

	struct CommndListExecuteDesc
	{
		Array<Ref<CommandList>> commandLists;
		Array<Ref<Semaphore>> waitSemaphores;
		Array<Ref<Semaphore>> signalSemaphores;
		Array<PipelineStage> waitDstPipelineStages;
		Array<uint64> waitValues;
		Array<uint64> signalValues;
	};

	/// Class repesenting a command buffer 
	class TYR_GRAPHICS_EXPORT CommandList
	{
	public:
		CommandList(const CommandListDesc& desc);
		virtual ~CommandList() = default;
		virtual void Begin(CommandBufferUsage usage) = 0;
		virtual void End() = 0;
		virtual void Reset(bool releaseResources) = 0;
		virtual void ClearColourImage(Ref<Image>& image, ImageLayout layout, const ClearColourValue& clearValue, const SubresourceRange* subresourceRanges, uint rangeCount) = 0;
		virtual void ClearDepthStencilImage(Ref<Image>& image, ImageLayout layout, const ClearDepthStencilValue& clearValue, const SubresourceRange* subresourceRanges, uint rangeCount) = 0;
		virtual void BeginRendering(const RenderingInfo& renderingInfo) = 0;
		virtual void EndRendering() = 0;
		// TODO: If phones are supported, add wrapper functions for vkCmdBeginRenderPass and vkCmdEndRenderPass for render passes
		virtual void AddBarriers(const BufferBarrier* bufferBarriers, uint bufferBarrierCount,
			const ImageBarrier* imageBarriers = nullptr, uint imageBarrierCount = 0u,
			const PipelineBarrier* pipelineBarriers = nullptr, uint pipelineBarrierCount = 0u,
			Dependency dependency = DEPENDENCY_UNKNOWN) = 0;
		virtual void CopyBuffer(const Ref<Buffer>& srcBuffer, const Ref<Buffer>& dstBuffer, const BufferCopyInfo* copyInfos, uint copyInfoCount) = 0;
		virtual void CopyBuffer(const Ref<Buffer>& srcBuffer, const Ref<Buffer>& dstBuffer) = 0;
		virtual void CopyBufferToImage(const Ref<Buffer>& buffer, const Ref<Image>& image, ImageLayout targetLayout, const BufferImageCopyInfo* regions, uint regionCount) = 0;
		virtual void CopyImage(const Ref<Image>& srcImage, ImageLayout srcLayout, const Ref<Image>& dstImage, ImageLayout dstLayout, const ImageCopyInfo* regions, uint regionCount) = 0;
		virtual void CopyImageToBuffer(const Ref<Image>& image, const Ref<Buffer>& buffer, ImageLayout currentLayout, const BufferImageCopyInfo* regions, uint regionCount) = 0;
		virtual void SetViewport(const Viewport* viewports, uint viewportCount) = 0;
		virtual void SetScissor(const GraphicsRect* scissors, uint scissorCount) = 0;
		virtual void BindPipeline(const Ref<Pipeline>& pipeline) = 0;
		virtual void BindVertexBuffers(const Ref<Buffer>* vertexBuffers, uint bufferCount) = 0;
		virtual void BindVertexBuffers(const Ref<BufferView>* vertexBufferViews, uint bufferViewCount) = 0;
		virtual void BindIndexBuffer(const Ref<Buffer>& indexBuffer, size_t offset = 0) = 0;
		virtual void BindIndexBuffer(const Ref<BufferView>& indexBufferView) = 0;
		virtual void BindDescriptorSetGroup(const Ref<DescriptorSetGroup>& group, const Ref<Pipeline>& pipeline) = 0;
		virtual void DrawIndexed(uint indexCount, uint instanceCount, uint firstIndex, int vertexOffset, uint firstInstance) = 0;
		virtual void Draw(uint vertexCount, uint instanceCount, uint firstVertex, uint firstInstance) = 0;
		virtual void DrawMeshTasks(uint groupCountX, uint groupCountY, uint groupCountZ) = 0;
		virtual void Dispatch(uint groupCountX, uint groupCountY, uint groupCountZ) = 0;
		virtual void Execute(const CommndListExecuteDesc* executeDescs, uint executeDescCount, uint queueIndex = 0u, const Ref<Fence>& fence = nullptr) = 0;

		const CommandListDesc& GetDesc() const { return m_Desc; }
		CommandQueueType GetQueueType() const { return m_QueueType; }
		uint GetQueueFamilyIndex() const { return m_QueueFamilyIndex; }

	protected:
		CommandListDesc m_Desc;
		CommandQueueType m_QueueType;
		uint m_QueueFamilyIndex;
	};
}
