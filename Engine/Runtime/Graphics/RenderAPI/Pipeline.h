

#pragma once

#include "GraphicsBase.h"
#include "RenderAPITypes.h"
#include "DescriptorSet.h"
#include "Geometry/GeometryTypes.h"
#include "Image.h"

namespace tyr
{
	class Shader;

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

	class RenderPass
	{
	public:
		RenderPass(const RenderPassDesc& desc);
		virtual ~RenderPass() = default;

		const RenderPassDesc& GetDesc() const { return m_Desc; }

	protected:
		RenderPassDesc m_Desc;
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
		LocalArray<Ref<DescriptorSetLayout>, 10> descriptorSetLayouts;
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
		LocalArray<DynamicState, 9> dynamicStates;
		PipelineLayoutDesc pipelineLayoutDesc;
		LocalArray<Ref<Shader>, 7> shaders;
	};

	struct ComputePipelineDesc
	{
		PipelineLayoutDesc pipelineLayoutDesc;
		Ref<Shader> shader;
	};

	// TODO: Fill this in when adding ray tracing support.
	struct RayTracingPipelineDesc
	{
		PipelineLayoutDesc pipelineLayoutDesc;
		LocalArray<Ref<Shader>, 7> shaders;
	};

	class TYR_GRAPHICS_EXPORT Pipeline
	{
	public:
		Pipeline() = default;
		virtual ~Pipeline() = default;

		virtual PipelineType GetPipelineType() const = 0;
	};

	class TYR_GRAPHICS_EXPORT GraphicsPipeline : public Pipeline
	{
	public:
		GraphicsPipeline(const GraphicsPipelineDesc& desc);
		virtual ~GraphicsPipeline() = default;
		
		PipelineType GetPipelineType() const override { return PipelineType::Graphics; } 

		const GraphicsPipelineDesc& GetDesc() const { return m_Desc; }

	protected:
		GraphicsPipelineDesc m_Desc;
	};

	class TYR_GRAPHICS_EXPORT ComputePipeline : public Pipeline
	{
	public:
		ComputePipeline(const ComputePipelineDesc& desc);
		virtual ~ComputePipeline() = default;

		PipelineType GetPipelineType() const override { return PipelineType::Compute; }

		const ComputePipelineDesc& GetDesc() const { return m_Desc; }

	protected:
		ComputePipelineDesc m_Desc;
	};

	class TYR_GRAPHICS_EXPORT RayTracingPipeline : public Pipeline
	{
	public:
		RayTracingPipeline(const RayTracingPipelineDesc& desc);
		virtual ~RayTracingPipeline() = default;

		PipelineType GetPipelineType() const override { return PipelineType::RayTracing; }

		const RayTracingPipelineDesc& GetDesc() const { return m_Desc; }

	protected:
		RayTracingPipelineDesc m_Desc;
	};
}
