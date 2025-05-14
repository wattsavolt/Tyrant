

#include "VulkanPipeline.h"
#include "VulkanDescriptorSet.h"
#include "VulkanShader.h"

namespace tyr
{
	VulkanRenderPass::VulkanRenderPass(VulkanDevice* device, const RenderPassDesc& renderPassDesc)
		: RenderPass(renderPassDesc)
		, m_Device(device)
	{
		StackAllocManager stack;

		VkAttachmentDescription* attachments = stack.Alloc<VkAttachmentDescription>(renderPassDesc.attachments.Size());
		for (int i = 0; i < renderPassDesc.attachments.Size(); ++i)
		{
			const AttachmentDesc& desc = renderPassDesc.attachments[i];
			VkAttachmentDescription& attachment = attachments[i];
			attachment.format = VulkanUtility::ToVulkanPixelFormat(desc.format);
			attachment.samples = VulkanUtility::ToVulkanSampleCount(desc.samples);
			attachment.loadOp = VulkanUtility::ToVulkanAttachmentLoadOp(desc.loadOp);
			attachment.storeOp = VulkanUtility::ToVulkanAttachmentStoreOp(desc.storeOp);
			attachment.stencilLoadOp = VulkanUtility::ToVulkanAttachmentLoadOp(desc.stencilLoadOp);
			attachment.stencilStoreOp = VulkanUtility::ToVulkanAttachmentStoreOp(desc.stencilStoreOp);
		}

		VkSubpassDescription* subpasses = stack.Alloc<VkSubpassDescription>(renderPassDesc.subpasses.Size());
		// Array<FixedArray<Array<VkAttachmentReference>, 5>> subpassAttachments;
		for (uint i = 0; i < renderPassDesc.subpasses.Size(); ++i)
		{
			const SubpassDesc& desc = renderPassDesc.subpasses[i];
			VkSubpassDescription& subpass = subpasses[i];
			subpass.pipelineBindPoint = VulkanUtility::ToVulkanPipelineBindPoint(desc.pipelineType);
			VkAttachmentReference* inputAttachments = stack.Alloc<VkAttachmentReference>(desc.inputAttachments.Size());
			VkAttachmentReference* colorAttachments = stack.Alloc<VkAttachmentReference>(desc.colorAttachments.Size());
			VkAttachmentReference* resolveAttachments = stack.Alloc<VkAttachmentReference>(desc.resolveAttachments.Size());
			VkAttachmentReference* depthStencilAttachments = stack.Alloc<VkAttachmentReference>(desc.depthStencilAttachments.Size());
			VkAttachmentReference* preserveAttachments = stack.Alloc<VkAttachmentReference>(desc.preserveAttachments.Size());
			CreateAttachmentReferences(desc.inputAttachments.Data(), desc.inputAttachments.Size(), inputAttachments);
			CreateAttachmentReferences(desc.colorAttachments.Data(), desc.colorAttachments.Size(), colorAttachments);
			CreateAttachmentReferences(desc.resolveAttachments.Data(), desc.resolveAttachments.Size(), resolveAttachments);
			CreateAttachmentReferences(desc.depthStencilAttachments.Data(), desc.depthStencilAttachments.Size(), depthStencilAttachments);
			CreateAttachmentReferences(desc.preserveAttachments.Data(), desc.preserveAttachments.Size(), preserveAttachments);
			subpass.inputAttachmentCount = desc.inputAttachments.Size();
			subpass.pInputAttachments = inputAttachments;
			subpass.colorAttachmentCount = desc.colorAttachments.Size();
			subpass.pColorAttachments = colorAttachments;
			subpass.pResolveAttachments = resolveAttachments;
			subpass.pDepthStencilAttachment = depthStencilAttachments;
			subpass.preserveAttachmentCount = desc.preserveAttachments.Size();
			subpass.pPreserveAttachments = reinterpret_cast<uint*>(preserveAttachments);
		}
		
		VkSubpassDependency* dependencies = stack.Alloc<VkSubpassDependency>(renderPassDesc.dependencies.Size());
		for (uint i = 0; i < renderPassDesc.dependencies.Size(); ++i)
		{
			const SubpassDependencyDesc& desc = renderPassDesc.dependencies[i];
			VkSubpassDependency dependency = dependencies[i];
			dependency.srcSubpass = desc.srcSubpass;
			dependency.dstSubpass = desc.dstSubpass;
			dependency.srcStageMask = static_cast<VkPipelineStageFlags>(desc.srcStageMask);
			dependency.dstStageMask = static_cast<VkPipelineStageFlags>(desc.dstStageMask);
			dependency.srcAccessMask = static_cast<VkAccessFlags>(desc.srcAccessMask);
			dependency.dstAccessMask = static_cast<VkAccessFlags>(desc.dstAccessMask);
			dependency.dependencyFlags = static_cast<VkDependencyFlags>(desc.dependencyFlags);
		}
		
		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.pNext = nullptr;
		renderPassInfo.attachmentCount = static_cast<uint>(renderPassDesc.attachments.Size());
		renderPassInfo.pAttachments = attachments;
		renderPassInfo.subpassCount = static_cast<uint>(renderPassDesc.subpasses.Size());
		renderPassInfo.pSubpasses = subpasses;
		renderPassInfo.dependencyCount = static_cast<uint>(renderPassDesc.dependencies.Size());
		renderPassInfo.pDependencies = dependencies;

		TYR_GASSERT(vkCreateRenderPass(device->GetLogicalDevice(), &renderPassInfo, g_VulkanAllocationCallbacks, &m_RenderPass));
	}

	VulkanRenderPass::~VulkanRenderPass()
	{
		vkDestroyRenderPass(m_Device->GetLogicalDevice(), m_RenderPass, g_VulkanAllocationCallbacks);
	}

	void VulkanRenderPass::CreateAttachmentReferences(const AttachmentReference* attachments, uint attachmentCount, VkAttachmentReference* vkAttachments)
	{
		for (uint i = 0; i < attachmentCount; ++i)
		{
			const AttachmentReference& attachment = attachments[i];
			vkAttachments[i].attachment = attachment.attachmentIndex;
			vkAttachments[i].layout = VulkanUtility::ToVulkanImageLayout(attachment.layout);
		}	
	}

	VulkanGraphicsPipeline::VulkanGraphicsPipeline(VulkanDevice* device, const GraphicsPipelineDesc& desc)
		: GraphicsPipeline(desc)
	{
		m_Device = device;

		StackAllocManager stack;

		VkDevice logicalDevice = m_Device->GetLogicalDevice();

		// Create vertex input state
		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.pNext = nullptr;
		vertexInputInfo.flags = 0;

		VkVertexInputBindingDescription* vkVertexBindingDescs = nullptr;
		vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint>(desc.vertexInputStateDesc.bindingDescriptions.Size());
		if (vertexInputInfo.vertexBindingDescriptionCount > 0)
		{
			vkVertexBindingDescs = stack.Alloc<VkVertexInputBindingDescription>(vertexInputInfo.vertexBindingDescriptionCount);
			for (uint i = 0; i < vertexInputInfo.vertexBindingDescriptionCount; ++i)
			{
				vkVertexBindingDescs[i].binding = desc.vertexInputStateDesc.bindingDescriptions[i].binding;
				vkVertexBindingDescs[i].stride = desc.vertexInputStateDesc.bindingDescriptions[i].stride;
				vkVertexBindingDescs[i].inputRate = VulkanUtility::ToVulkanVertexInputRate(desc.vertexInputStateDesc.bindingDescriptions[i].inputRate);
			}
		}
		vertexInputInfo.pVertexBindingDescriptions = vkVertexBindingDescs;

		VkVertexInputAttributeDescription* vkVertexAttributeDescs = nullptr;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint>(desc.vertexInputStateDesc.attributeDescriptions.Size());
		if (vertexInputInfo.vertexAttributeDescriptionCount > 0)
		{
			vkVertexAttributeDescs = stack.Alloc<VkVertexInputAttributeDescription>(vertexInputInfo.vertexAttributeDescriptionCount);
			for (uint i = 0; i < vertexInputInfo.vertexAttributeDescriptionCount; ++i)
			{
				vkVertexAttributeDescs[i].location = desc.vertexInputStateDesc.attributeDescriptions[i].location;
				vkVertexAttributeDescs[i].binding = desc.vertexInputStateDesc.attributeDescriptions[i].binding;
				vkVertexAttributeDescs[i].format = VulkanUtility::ToVulkanPixelFormat(desc.vertexInputStateDesc.attributeDescriptions[i].format);
				vkVertexAttributeDescs[i].offset = desc.vertexInputStateDesc.attributeDescriptions[i].offset;
			}
		}
		vertexInputInfo.pVertexAttributeDescriptions = vkVertexAttributeDescs;

		// Create input assembly
		VkPipelineInputAssemblyStateCreateInfo inputAssembly;
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.pNext = nullptr;
		inputAssembly.topology = VulkanUtility::ToVulkanPrimitiveTopology(desc.topology);
		inputAssembly.primitiveRestartEnable = VK_FALSE;
		inputAssembly.flags = 0;

		// Create viewport state
		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.pNext = nullptr;
		viewportState.flags = 0;
		viewportState.viewportCount = 1; // Spec says this needs to be at least 1.
		viewportState.scissorCount = 1;
		// Below are not needed because these can be set using the viewport dynamic state this wrapper adds automatically.
		viewportState.pViewports = nullptr; // Dynamic
		viewportState.pScissors = nullptr; // Dynamic


		// Create rasterizer state
		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.pNext = nullptr;
		rasterizer.depthClampEnable = desc.rasterizerStateDesc.depthClampEnabled;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = VulkanUtility::ToVulkanPolygonMode(desc.rasterizerStateDesc.polygonMode);
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VulkanUtility::ToVulkanCullMode(desc.rasterizerStateDesc.cullMode);
		rasterizer.frontFace = VulkanUtility::ToVulkanFrontFace(desc.rasterizerStateDesc.frontFace);
		rasterizer.depthBiasEnable = desc.rasterizerStateDesc.depthBiasEnabled;
		rasterizer.depthBiasConstantFactor = desc.rasterizerStateDesc.depthBiasConstantFactor;
		rasterizer.depthBiasClamp = desc.rasterizerStateDesc.depthBiasClamp;
		rasterizer.depthBiasSlopeFactor = desc.rasterizerStateDesc.depthBiasSlopeFactor;

		// Create multi-sampling state
		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.flags = 0;
		multisampling.rasterizationSamples = VulkanUtility::ToVulkanSampleCount(desc.multiSampleDesc.rasterizationSamples);
		multisampling.sampleShadingEnable = desc.multiSampleDesc.sampleShadingEnable;
		multisampling.minSampleShading = desc.multiSampleDesc.minSampleShading;
		// TODO: Set this when multisampling enabled
		multisampling.pSampleMask = nullptr; // Usually one bit per sample: e.g. 0x0000000F to enable all samples in a 4-sample setup
		multisampling.alphaToCoverageEnable = desc.multiSampleDesc.alphaToCoverageEnable; // Get from blend state alphaToCoverageEnabled
		multisampling.alphaToOneEnable = desc.multiSampleDesc.alphaToOneEnable;

		// Create blend state
		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask = desc.blendStateDesc.colorWriteMask & 0xF;
		colorBlendAttachment.blendEnable = desc.blendStateDesc.blendEnabled;
		colorBlendAttachment.srcColorBlendFactor = VulkanUtility::ToVulkanBlendFactor(desc.blendStateDesc.srcColorBlendFactor);
		colorBlendAttachment.dstColorBlendFactor = VulkanUtility::ToVulkanBlendFactor(desc.blendStateDesc.destColorBlendFactor);
		colorBlendAttachment.colorBlendOp = VulkanUtility::ToVulkanBlendOp(desc.blendStateDesc.colorBlendOp);
		colorBlendAttachment.srcAlphaBlendFactor = VulkanUtility::ToVulkanBlendFactor(desc.blendStateDesc.srcAlphaBlendFactor);
		colorBlendAttachment.dstAlphaBlendFactor = VulkanUtility::ToVulkanBlendFactor(desc.blendStateDesc.destAlphaBlendFactor);
		colorBlendAttachment.alphaBlendOp = VulkanUtility::ToVulkanBlendOp(desc.blendStateDesc.alphaBlendOp);

		VkPipelineColorBlendStateCreateInfo blendState{};
		blendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		blendState.pNext = nullptr;
		blendState.logicOpEnable = VK_FALSE;
		blendState.logicOp = VK_LOGIC_OP_NO_OP;
		blendState.attachmentCount = 1;
		blendState.pAttachments = &colorBlendAttachment;
		blendState.blendConstants[0] = 0.0f;
		blendState.blendConstants[1] = 0.0f;
		blendState.blendConstants[2] = 0.0f;
		blendState.blendConstants[3] = 0.0f;

		// Create depth stencil state
		VkPipelineDepthStencilStateCreateInfo depthStencilState;
		depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilState.pNext = nullptr;
		depthStencilState.depthTestEnable = desc.depthStencilStateDesc.depthTestEnable;
		depthStencilState.depthCompareOp = VulkanUtility::ToVulkanCompareOp(desc.depthStencilStateDesc.depthCompareOp);
		depthStencilState.depthBoundsTestEnable = desc.depthStencilStateDesc.depthBoundsTestEnable;
		depthStencilState.stencilTestEnable = desc.depthStencilStateDesc.stencilTestEnable;
		ToVulkanStencilOpState(desc.depthStencilStateDesc.front, depthStencilState.front);
		ToVulkanStencilOpState(desc.depthStencilStateDesc.back, depthStencilState.back);
		depthStencilState.minDepthBounds = desc.depthStencilStateDesc.minDepthBounds;
		depthStencilState.maxDepthBounds = desc.depthStencilStateDesc.maxDepthBounds;


		// Create pipeline layout
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.pNext = nullptr;
		pipelineLayoutInfo.flags = 0; // Maybe use later - VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT

		VkDescriptorSetLayout* vkDescriptorSetLayouts = nullptr;
		pipelineLayoutInfo.setLayoutCount = static_cast<uint>(desc.pipelineLayoutDesc.descriptorSetLayouts.Size());
		if (pipelineLayoutInfo.setLayoutCount > 0)
		{
			vkDescriptorSetLayouts = stack.Alloc<VkDescriptorSetLayout>(pipelineLayoutInfo.setLayoutCount);
			for (uint i = 0; i < pipelineLayoutInfo.setLayoutCount; ++i)
			{
				Ref<VulkanDescriptorSetLayout> const& layout = RefCast<VulkanDescriptorSetLayout>(desc.pipelineLayoutDesc.descriptorSetLayouts[i]);
				vkDescriptorSetLayouts[i] = layout->GetLayout();
			}
		}
		pipelineLayoutInfo.pSetLayouts = vkDescriptorSetLayouts;
	
		VkPushConstantRange* vkConstantRanges = nullptr;
		pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint>(desc.pipelineLayoutDesc.pushConstantRanges.Size());
		if (pipelineLayoutInfo.pushConstantRangeCount > 0)
		{
			vkConstantRanges = stack.Alloc<VkPushConstantRange>(pipelineLayoutInfo.pushConstantRangeCount);
			for (uint i = 0; i < pipelineLayoutInfo.pushConstantRangeCount; ++i)
			{
				const PushConstantRange& range = desc.pipelineLayoutDesc.pushConstantRanges[i];
				vkConstantRanges[i].stageFlags = range.stageFlags;
				vkConstantRanges[i].offset = range.offset;
				vkConstantRanges[i].size = range.size;
			}
		}
		pipelineLayoutInfo.pPushConstantRanges = vkConstantRanges;

		TYR_GASSERT(vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, g_VulkanAllocationCallbacks, &m_PipelineLayout));


		// Note: m_Desc must be used below for dynamic states as some dyanmic states are added if not present in description argument.
		VkDynamicState* vkDynamicStates = nullptr;
		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.pNext = nullptr;
		dynamicState.dynamicStateCount = m_Desc.dynamicStates.Size();
		if (dynamicState.dynamicStateCount > 0)
		{
			vkDynamicStates = stack.Alloc<VkDynamicState>(dynamicState.dynamicStateCount);
			for (uint i = 0; i < dynamicState.dynamicStateCount; ++i)
			{
				vkDynamicStates[i] = VulkanUtility::ToVulkanDynamicState(m_Desc.dynamicStates[i]);
			}
		}
		dynamicState.pDynamicStates = vkDynamicStates;


		// Create shader stages
		VkPipelineShaderStageCreateInfo* vkStages = nullptr;
		uint shaderCount = desc.shaders.Size();
		if (shaderCount > 0)
		{
			vkStages = stack.Alloc<VkPipelineShaderStageCreateInfo>(shaderCount);
			for (uint i = 0; i < shaderCount; ++i)
			{
				const Ref<VulkanShader>& shader = RefCast<VulkanShader>(desc.shaders[i]);
				vkStages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
				vkStages[i].pNext = nullptr;
				vkStages[i].flags = 0;
				vkStages[i].stage = static_cast<VkShaderStageFlagBits>(shader->GetStage());
				vkStages[i].module = shader->GetShaderModule();
				vkStages[i].pName = shader->GetEntryPoint();
				vkStages[i].pSpecializationInfo = nullptr;
			}
		}

		// Create pipeline
		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = shaderCount;
		pipelineInfo.pStages = vkStages;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = &depthStencilState;
		pipelineInfo.pColorBlendState = &blendState;
		pipelineInfo.pDynamicState = &dynamicState;
		pipelineInfo.layout = m_PipelineLayout;

#if TYR_USE_DYNAMIC_RENDERING
		pipelineInfo.renderPass = VK_NULL_HANDLE;
		VkPipelineRenderingCreateInfo pipelineRendering;
		pipelineRendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
		pipelineRendering.pNext = nullptr;
		pipelineRendering.viewMask = desc.dynamicRendering.viewMask;
		pipelineRendering.colorAttachmentCount = desc.dynamicRendering.colorAttachmentFormats.Size();
		VkFormat* vkColorAttachmentFormats = stack.Alloc<VkFormat>(pipelineRendering.colorAttachmentCount);
		for (uint i = 0; i < pipelineRendering.colorAttachmentCount; ++i)
		{
			vkColorAttachmentFormats[i] = VulkanUtility::ToVulkanPixelFormat(desc.dynamicRendering.colorAttachmentFormats[i]);
		}
		pipelineRendering.pColorAttachmentFormats = vkColorAttachmentFormats;
		pipelineRendering.depthAttachmentFormat = VulkanUtility::ToVulkanPixelFormat(desc.dynamicRendering.depthAttachmentFormat);
		pipelineRendering.stencilAttachmentFormat = VulkanUtility::ToVulkanPixelFormat(desc.dynamicRendering.stencilAttachmentFormat);
		pipelineInfo.pNext = &pipelineRendering;
#else
		Ref<VulkanRenderPass> renderPass = RefCast<VulkanRenderPass>(desc.renderPass);
		pipelineInfo.renderPass = renderPass->GetRenderPass();
		pipelineInfo.subpass = desc.subpassIndex;
		pipelineInfo.pNext = nullptr;
#endif

		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; 
		pipelineInfo.basePipelineIndex = -1; 

		TYR_GASSERT(vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, g_VulkanAllocationCallbacks, &m_Pipeline));
	}

	VulkanGraphicsPipeline::~VulkanGraphicsPipeline()
	{
		vkDestroyPipeline(m_Device->GetLogicalDevice(), m_Pipeline, g_VulkanAllocationCallbacks);
		vkDestroyPipelineLayout(m_Device->GetLogicalDevice(), m_PipelineLayout, g_VulkanAllocationCallbacks);
	}

	void VulkanGraphicsPipeline::ToVulkanStencilOpState(const StencilOpState& stencilOpState, VkStencilOpState& vkStencilOpState)
	{
		vkStencilOpState.failOp = VulkanUtility::ToVulkanStencilOp(stencilOpState.failOp);
		vkStencilOpState.passOp = VulkanUtility::ToVulkanStencilOp(stencilOpState.passOp);
		vkStencilOpState.depthFailOp = VulkanUtility::ToVulkanStencilOp(stencilOpState.depthFailOp);
		vkStencilOpState.compareOp = VulkanUtility::ToVulkanCompareOp(stencilOpState.compareOp);
		vkStencilOpState.compareMask = stencilOpState.compareMask;
		vkStencilOpState.writeMask = stencilOpState.writeMask;
		vkStencilOpState.reference = stencilOpState.reference;
	}

	VulkanComputePipeline::VulkanComputePipeline(VulkanDevice* device, const ComputePipelineDesc& desc)
		: ComputePipeline(desc)
	{
		m_Device = device;

		StackAllocManager stack;

		VkDevice logicalDevice = m_Device->GetLogicalDevice();

		// TODO: Finish constructor
	}

	VulkanComputePipeline::~VulkanComputePipeline()
	{
		vkDestroyPipeline(m_Device->GetLogicalDevice(), m_Pipeline, g_VulkanAllocationCallbacks);
		vkDestroyPipelineLayout(m_Device->GetLogicalDevice(), m_PipelineLayout, g_VulkanAllocationCallbacks);
	}

	VulkanRayTracingPipeline::VulkanRayTracingPipeline(VulkanDevice* device, const RayTracingPipelineDesc& desc)
		: RayTracingPipeline(desc)
	{
		m_Device = device;

		StackAllocManager stack;

		VkDevice logicalDevice = m_Device->GetLogicalDevice();

		// TODO: Finish constructor
	}

	VulkanRayTracingPipeline::~VulkanRayTracingPipeline()
	{
		vkDestroyPipeline(m_Device->GetLogicalDevice(), m_Pipeline, g_VulkanAllocationCallbacks);
		vkDestroyPipelineLayout(m_Device->GetLogicalDevice(), m_PipelineLayout, g_VulkanAllocationCallbacks);
	}
}