

#pragma once

#include "GraphicsBase.h"
#include "RenderAPITypes.h"
#include "DescriptorSetGroup.h"
#include "Geometry/GeometryTypes.h"
#include "Image.h"
#include "ShaderModule.h"

namespace tyr
{
	using RenderPassHandle = ResourceHandle;
	struct GraphicsPipelineHandle : public ResourceHandle {};
	struct ComputePipelineHandle : public ResourceHandle {};
	struct RayTracingPipelineHandle : public ResourceHandle {};

	struct VertexInputBindingDesc
	{
		uint binding;
		uint stride;
		VertexInputRate inputRate;
	};

	struct VertexInputAttributeDesc
	{
		uint binding;
		uint location;
		PixelFormat format;
		uint offset;
	};

	struct VertexInputStateDesc
	{
		LocalArray<VertexInputBindingDesc, 8> bindingDescriptions;
		LocalArray<VertexInputAttributeDesc, 16> attributeDescriptions;
	};

	struct AttachmentDesc
	{
		PixelFormat format;
		SampleCount	samples;
		AttachmentLoadOp loadOp;
		AttachmentStoreOp storeOp;
		AttachmentLoadOp stencilLoadOp;
		AttachmentStoreOp stencilStoreOp;
		ImageLayout initialLayout;
		ImageLayout finalLayout;
	};

	struct AttachmentReference
	{
		uint attachmentIndex;	
		ImageLayout layout;
	};

	struct SubpassDesc
	{
		PipelineType pipelineType;
		LocalArray<AttachmentReference, 4>	inputAttachments; // Attachments to be used as an input colour (Read in per-pixel).
		LocalArray<AttachmentReference, 4>	colorAttachments; // Attachments for the output colour.
		LocalArray<AttachmentReference, 4>	resolveAttachments;	// Attachments for the multisample colour attachments to resolve to (if needed), which should be related by index to their respective colorAttachment.
		LocalArray<AttachmentReference, 1>	depthStencilAttachments; // Attachments to be used for the depth-stencil. Only 1 allowed.
		LocalArray<AttachmentReference, 4>	preserveAttachments;	// Attachments that are needed in later subpasses, but not this one.
	};

	struct SubpassDependencyDesc
	{
		uint srcSubpass; // Index to a previous subpass that this subpass depends on.
		uint dstSubpass; // Index of this subpass.
		PipelineStage srcStageMask; // The stages of the source subpass that need to be finished.
		PipelineStage dstStageMask; // The stages of this subapass that need waiting.
		BarrierAccess srcAccessMask; // The accesses that are needed into the previous subpass stage.
		BarrierAccess dstAccessMask; // The accesses that are needed into this subpass stage.
		Dependency dependencyFlags; // The rendering localities required between subpasses.
	};

	struct RenderPassDesc
	{
		LocalArray<AttachmentDesc, 8> attachments;
		LocalArray<SubpassDesc, 6> subpasses;
		LocalArray<SubpassDependencyDesc, 10> dependencies;
	};

	// To simplify, this maps to VkPipelineColorBlendAttachmentState and not VkPipelineColorBlendStateCreateInfo for Vulkan  
	// as there should only be a need for one attachment.
	struct BlendStateDesc
	{
		BlendFactor srcColorBlendFactor;
		BlendFactor destColorBlendFactor;
		BlendOp colorBlendOp;
		BlendFactor srcAlphaBlendFactor;
		BlendFactor destAlphaBlendFactor;
		BlendOp alphaBlendOp;
		ColorComponent colorWriteMask;
		bool blendEnabled;
	};

	struct RasterizerStateDesc
	{
		float depthBiasConstantFactor; 
		float depthBiasClamp; 
		float depthBiasSlopeFactor; 
		CullMode cullMode;
		PolygonMode polygonMode;
		FrontFace frontFace;
		bool depthClampEnabled;
		bool depthBiasEnabled; // Sometimes used for shadow mapping.
	};

	struct StencilOpState
	{
		StencilOp failOp;
		StencilOp passOp;
		StencilOp depthFailOp;
		CompareOp compareOp;
		uint compareMask;
		uint writeMask;
		uint reference;
	};

	struct DepthStencilStateDesc
	{
		CompareOp depthCompareOp;
		StencilOpState front;
		StencilOpState back;
		float minDepthBounds;
		float maxDepthBounds;
		bool depthTestEnable;
		bool depthWriteEnable;
		bool depthBoundsTestEnable;
		bool stencilTestEnable;
	};

	struct MultiSampleStateDesc
	{
		SampleCount rasterizationSamples; 
		float minSampleShading; // Fraction of samples to run full shading for when sampleShadingEnable is enabled ie. 1.0f to run for all.
		bool sampleShadingEnable; // Performs shading per sample instead of per pixel when enabled ie. FSAA which is more expensive.
		bool alphaToCoverageEnable;
		bool alphaToOneEnable;
	};

	struct PushConstantRange
	{
		ShaderStage stageFlags;
		uint offset;
		uint size;
	};

	struct PipelineLayoutDesc
	{
		LocalArray<DescriptorSetLayoutHandle, 10> descriptorSetLayouts;
		// Not supported in HLSL
		LocalArray<PushConstantRange, 5> pushConstantRanges;
	};

	struct DynamicRendering
	{
		// Set to 0x1 to render to first view if multi-view feature is enabled. 
		// Default is 0 which means it can render to all views in the render pass.
		uint viewMask = 0;
		LocalArray<PixelFormat, 4> colorAttachmentFormats;
		PixelFormat depthAttachmentFormat;
		PixelFormat stencilAttachmentFormat;
	};

	struct GraphicsPipelineDesc
	{
		static constexpr uint8 c_MaxDynamicStates = 9;
		static constexpr uint8 c_MaxShaders = 7;

		VertexInputStateDesc vertexInputStateDesc;
		PrimitiveTopology topology;
		BlendStateDesc blendStateDesc;
		RasterizerStateDesc rasterizerStateDesc;
		DepthStencilStateDesc depthStencilStateDesc;
		MultiSampleStateDesc multiSampleDesc;
#if TYR_USE_DYNAMIC_RENDERING
		DynamicRendering dynamicRendering;
#else
		Ref<RenderPass> renderPass;
		uint subpassIndex;
#endif
		LocalArray<DynamicState, c_MaxDynamicStates> dynamicStates;
		PipelineLayoutDesc pipelineLayoutDesc;
		LocalArray<ShaderModuleHandle, c_MaxShaders> shaders;
	};

	struct ComputePipelineDesc
	{
		PipelineLayoutDesc pipelineLayoutDesc;
		ShaderModuleHandle shader;
	};

	// TODO: Finish this when adding ray tracing support.
	struct RayTracingPipelineDesc
	{
		PipelineLayoutDesc pipelineLayoutDesc;
		LocalArray<ShaderModuleHandle, 7> shaders;
	};
}
