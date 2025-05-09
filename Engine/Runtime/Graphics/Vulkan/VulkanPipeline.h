#pragma once

#include "RenderAPI/Pipeline.h"
#include "VulkanDevice.h"

namespace tyr
{
	class VulkanRenderPass final : public RenderPass
	{
	public:
		VulkanRenderPass(VulkanDevice* device, const RenderPassDesc& renderPassDesc);
		~VulkanRenderPass();

		VkRenderPass GetRenderPass() { return m_RenderPass; }
	private:
		void CreateAttachmentReferences(const AttachmentReference* attachments, uint attachmentCount, VkAttachmentReference* vkAttachments);

		VulkanDevice* m_Device;
		VkRenderPass m_RenderPass;
	};

	class VulkanGraphicsPipeline final : public GraphicsPipeline
	{
	public:
		VulkanGraphicsPipeline(VulkanDevice* device, const GraphicsPipelineDesc& desc);
		~VulkanGraphicsPipeline(); 
		static void ToVulkanStencilOpState(const StencilOpState& stencilOpState, VkStencilOpState& vkStencilOpState);

		VkPipeline GetPipeline() const { return m_Pipeline; }
		VkPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }

	private:
		VulkanDevice* m_Device;
		VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
		VkPipeline m_Pipeline = VK_NULL_HANDLE;
	};

	class VulkanComputePipeline final : public ComputePipeline
	{
	public:
		VulkanComputePipeline(VulkanDevice* device, const ComputePipelineDesc& desc);
		~VulkanComputePipeline();

		VkPipeline GetPipeline() const { return m_Pipeline; }
		VkPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }

	private:
		VulkanDevice* m_Device;
		VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
		VkPipeline m_Pipeline = VK_NULL_HANDLE;
	};

	class VulkanRayTracingPipeline final : public RayTracingPipeline
	{
	public:
		VulkanRayTracingPipeline(VulkanDevice* device, const RayTracingPipelineDesc& desc);
		~VulkanRayTracingPipeline();

		VkPipeline GetPipeline() const { return m_Pipeline; }
		VkPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }

	private:
		VulkanDevice* m_Device;
		VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
		VkPipeline m_Pipeline = VK_NULL_HANDLE;
	};
}
