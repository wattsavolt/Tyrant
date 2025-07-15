
#pragma once

#include "RenderAPI/CommandList.h"
#include "VulkanDevice.h"

namespace tyr
{
	class VulkanBuffer;
	class RenderingInfo;
	class VulkanCommandList final : public CommandList
	{
	public:
		VulkanCommandList(VulkanDevice& device, const CommandListDesc& desc);
		~VulkanCommandList();
		void Begin(CommandBufferUsage usage) override;
		void End() override;
		void Reset(bool releaseResources) override;
		void ClearColourImage(ORef<Image>& image, ImageLayout layout, const ClearColourValue& clearValue, const SubresourceRange* subresourceRanges, uint rangeCount) override;
		void ClearDepthStencilImage(ORef<Image>& image, ImageLayout layout, const ClearDepthStencilValue& clearValue, const SubresourceRange* subresourceRanges, uint rangeCount) override;
		void BeginRendering(const RenderingInfo& renderingInfo) override;
		void EndRendering() override;
		/// @note Uses barrier2
		void AddBarriers(const BufferBarrier* bufferBarriers, uint bufferBarrierCount,
			const ImageBarrier* imageBarriers = nullptr, uint imageBarrierCount = 0u,
			const PipelineBarrier* pipelineBarriers = nullptr, uint pipelineBarrierCount = 0u,
			Dependency dependency = DEPENDENCY_UNKNOWN) override;
		void CopyBuffer(const ORef<Buffer>& srcBuffer, const ORef<Buffer>& dstBuffer, const BufferCopyInfo* copyInfos, uint copyInfoCount) override;
		void CopyBuffer(const ORef<Buffer>& srcBuffer, const ORef<Buffer>& dstBuffer) override;
		void CopyBufferToImage(const ORef<Buffer>& buffer, const ORef<Image>& image, ImageLayout targetLayout, const BufferImageCopyInfo* regions, uint regionCount) override;
		void CopyImage(const ORef<Image>& srcImage, ImageLayout srcLayout, const ORef<Image>& dstImage, ImageLayout dstLayout, const ImageCopyInfo* regions, uint regionCount) override;
		void CopyImageToBuffer(const ORef<Image>& image, const ORef<Buffer>& buffer, ImageLayout currentLayout, const BufferImageCopyInfo* regions, uint regionCount) override;
		void SetViewport(const Viewport* viewports, uint viewportCount) override;
		void SetScissor(const GraphicsRect* scissors, uint scissorCount) override;
		void BindPipeline(const Ref<Pipeline>& pipeline) override;
		void BindVertexBuffers(const Buffer* const* vertexBuffers, uint bufferCount) override;
		void BindVertexBuffers(const BufferView* const* vertexBufferViews, uint bufferViewCount) override;
		void BindIndexBuffer(const ORef<Buffer>& indexBuffer, size_t offset = 0) override;
		void BindIndexBuffer(const ORef<BufferView>& vertexBufferView) override;
		void BindDescriptorSetGroup(const Ref<DescriptorSetGroup>& group, const Ref<Pipeline>& pipeline) override;
		void DrawIndexed(uint indexCount, uint instanceCount, uint firstIndex, int vertexOffset, uint firstInstance) override;
		void Draw(uint vertexCount, uint instanceCount, uint firstVertex, uint firstInstance) override;
		void DrawMeshTasks(uint groupCountX, uint groupCountY, uint groupCountZ) override;
		void Dispatch(uint groupCountX, uint groupCountY, uint groupCountZ) override;
		void Execute(const CommndListExecuteDesc* executeDescs, uint executeDescCount, uint queueIndex = 0u, const Ref<Fence>& fence = nullptr) override;

	private:
		void BindIndexBufferInternal(const VulkanBuffer* buffer, size_t offset);

		VulkanDevice& m_Device;
		VkCommandBuffer m_CommandBuffer;
		VkCommandPool m_CommandPool;
	};
}
