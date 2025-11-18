

#include "VulkanUtility.h"

namespace tyr
{
	VkFormat VulkanUtility::ToVulkanPixelFormat(PixelFormat format)
	{
		switch (format)
		{
		case PF_UNKNOWN:
			return VK_FORMAT_UNDEFINED;
		case PF_R8G8B8A8_UNORM:
			return VK_FORMAT_R8G8B8A8_UNORM;
		case PF_R8G8B8A8_SNORM:
			return VK_FORMAT_R8G8B8A8_SNORM;
		case PF_R8G8B8A8_USCALED:
			return VK_FORMAT_R8G8B8A8_USCALED;
		case PF_R8G8B8A8_SSCALED:
			return VK_FORMAT_R8G8B8A8_SSCALED;
		case PF_R8G8B8A8_UINT:
			return VK_FORMAT_R8G8B8A8_UINT;
		case PF_R8G8B8A8_SINT:
			return VK_FORMAT_R8G8B8A8_SINT;
		case PF_R8G8B8A8_SRGB:
			return VK_FORMAT_R8G8B8A8_SRGB;

		case PF_R16G16B16A16_UNORM:
			return VK_FORMAT_R16G16B16A16_UNORM;
		case PF_R16G16B16A16_SNORM:
			return VK_FORMAT_R16G16B16A16_SNORM;
		case PF_R16G16B16A16_USCALED:
			return VK_FORMAT_R16G16B16A16_USCALED;
		case PF_R16G16B16A16_SSCALED:
			return VK_FORMAT_R16G16B16A16_SSCALED;
		case PF_R16G16B16A16_UINT:
			return VK_FORMAT_R16G16B16A16_UINT;
		case PF_R16G16B16A16_SINT:
			return VK_FORMAT_R16G16B16A16_SINT;
		case PF_R16G16B16A16_SFLOAT:
			return VK_FORMAT_R16G16B16A16_SFLOAT;

		case PF_R32G32B32_UINT:
			return VK_FORMAT_R32G32B32_UINT;
		case PF_R32G32B32_SINT:
			return VK_FORMAT_R32G32B32_SINT;
		case PF_R32G32B32_SFLOAT:
			return VK_FORMAT_R32G32B32_SFLOAT;
		case PF_R32G32B32A32_UINT:
			return VK_FORMAT_R32G32B32A32_UINT;
		case PF_R32G32B32A32_SINT:
			return VK_FORMAT_R32G32B32A32_SINT;
		case PF_R32G32B32A32_SFLOAT:
			return VK_FORMAT_R32G32B32A32_SFLOAT;

		case PF_B8G8R8A8_UNORM:
			return VK_FORMAT_R8G8B8A8_UNORM;
		case PF_B8G8R8A8_SNORM:
			return VK_FORMAT_R8G8B8A8_SNORM;
		case PF_B8G8R8A8_USCALED:
			return VK_FORMAT_R8G8B8A8_USCALED;
		case PF_B8G8R8A8_SSCALED:
			return VK_FORMAT_R8G8B8A8_SSCALED;
		case PF_B8G8R8A8_UINT:
			return VK_FORMAT_R8G8B8A8_UINT;
		case PF_B8G8R8A8_SINT:
			return VK_FORMAT_R8G8B8A8_SINT;
		case PF_B8G8R8A8_SRGB:
			return VK_FORMAT_R8G8B8A8_SRGB;

		case PF_B16G16R16A16_UNORM:
			return VK_FORMAT_R16G16B16A16_UNORM;
		case PF_B16G16R16A16_SNORM:
			return VK_FORMAT_R16G16B16A16_SNORM;
		case PF_B16G16R16A16_USCALED:
			return VK_FORMAT_R16G16B16A16_USCALED;
		case PF_B16G16R16A16_SSCALED:
			return VK_FORMAT_R16G16B16A16_SSCALED;
		case PF_B16G16R16A16_UINT:
			return VK_FORMAT_R16G16B16A16_UINT;
		case PF_B16G16R16A16_SINT:
			return VK_FORMAT_R16G16B16A16_SINT;
		case PF_B16G16R16A16_SFLOAT:
			return VK_FORMAT_R16G16B16A16_SFLOAT;

		case PF_D16_UNORM:
			return VK_FORMAT_D16_UNORM;
		case PF_D24_UNORM_S8_UINT:
			return VK_FORMAT_D24_UNORM_S8_UINT;
		case PF_D32_SFLOAT:
			return VK_FORMAT_D32_SFLOAT;
		case PF_D32_FLOAT_S8_UINT:
			return VK_FORMAT_D32_SFLOAT_S8_UINT;

		case PF_BC3_UNORM:
			return VK_FORMAT_BC3_UNORM_BLOCK;
		case PF_BC3_SRGB:
			return VK_FORMAT_BC3_SRGB_BLOCK;
		case PF_BC5_UNORM:
			return VK_FORMAT_BC5_UNORM_BLOCK;
		case PF_BC5_SNORM:
			return VK_FORMAT_BC5_SNORM_BLOCK;
		case PF_BC7_UNORM:
			return VK_FORMAT_BC7_UNORM_BLOCK;
		case PF_BC7_SRGB:
			return VK_FORMAT_BC7_SRGB_BLOCK;
		default:
			TYR_ASSERT(false);
			return VK_FORMAT_UNDEFINED;
		}
	}

	VkColorSpaceKHR VulkanUtility::ToVulkanColorSpace(ColorSpace colorSpace)
	{
		switch (colorSpace)
		{
		case CP_SRGB_NONLINEAR:
			return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		case CP_BT709_LINEAR:
			return VK_COLOR_SPACE_BT709_LINEAR_EXT;
		case CP_BT709_NONLINEAR:
			return VK_COLOR_SPACE_BT709_NONLINEAR_EXT;
		case CP_BT2020_LINEAR:
			return VK_COLOR_SPACE_BT2020_LINEAR_EXT;
		default:
			TYR_ASSERT(false);
			return VK_COLOR_SPACE_MAX_ENUM_KHR;
		}
	}

	VkImageLayout VulkanUtility::ToVulkanImageLayout(ImageLayout layout)
	{
		switch (layout)
		{
		case IMAGE_LAYOUT_UNKNOWN:
			return VK_IMAGE_LAYOUT_UNDEFINED;
		case IMAGE_LAYOUT_GENERAL:
			return VK_IMAGE_LAYOUT_GENERAL;
		case IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		case IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		case IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
			return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		case IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		case IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		case IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		case IMAGE_LAYOUT_PREINITIALISED:
			return VK_IMAGE_LAYOUT_PREINITIALIZED;
		case IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
			return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
		case IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
			return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;
		case IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
			return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
		case IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
			return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
		case IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL:
			return VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
		case IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL:
			return VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL;
		case IMAGE_LAYOUT_READ_ONLY_OPTIMAL:
			return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
		case IMAGE_LAYOUT_ATTACHMENT_OPTIMAL:
			return VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		case IMAGE_LAYOUT_PRESENT_SRC:
			return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		case IMAGE_LAYOUT_SHARED_PRESENT:
			return VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR;
		case IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL:
			return VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;
		default:
			TYR_ASSERT(false);
			return VK_IMAGE_LAYOUT_MAX_ENUM;
		}
	}

	VkPrimitiveTopology VulkanUtility::ToVulkanPrimitiveTopology(PrimitiveTopology topology)
	{
		switch (topology)
		{
		case PrimitiveTopology::PointList:
			return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		case PrimitiveTopology::LineList:
			return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
		case PrimitiveTopology::LineStrip:
			return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
		case PrimitiveTopology::TriangeList:
			return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		case PrimitiveTopology::TriangleStrip:
			return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		default:
			TYR_ASSERT(false);
			return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
		}
	}

	VkCullModeFlagBits VulkanUtility::ToVulkanCullMode(CullMode mode)
	{
		switch (mode)
		{
		case CullMode::None:
			return VK_CULL_MODE_NONE;
		case CullMode::Front:
			return VK_CULL_MODE_FRONT_BIT;
		case CullMode::Back:
			return VK_CULL_MODE_BACK_BIT;
		case CullMode::FrontAndBack:
			return VK_CULL_MODE_FRONT_AND_BACK;
		default:
			TYR_ASSERT(false);
			return VK_CULL_MODE_FLAG_BITS_MAX_ENUM;
		}
	}

	VkSampleCountFlagBits VulkanUtility::ToVulkanSampleCount(SampleCount sampleCount)
	{
		switch (sampleCount)
		{
		case SampleCount::OneBit:
			return VK_SAMPLE_COUNT_1_BIT;
		case SampleCount::TwoBit:
			return VK_SAMPLE_COUNT_2_BIT;
		case SampleCount::FourBit:
			return VK_SAMPLE_COUNT_4_BIT;
		case SampleCount::EightBit:
			return VK_SAMPLE_COUNT_8_BIT;
		case SampleCount::SixteenBit:
			return VK_SAMPLE_COUNT_16_BIT;
		case SampleCount::ThirtyTwoBit:
			return VK_SAMPLE_COUNT_32_BIT;
		case SampleCount::SixtyFourBit:
			return VK_SAMPLE_COUNT_64_BIT;
		default:
			TYR_ASSERT(false);
			return VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM;
		}
	}

	VkBlendFactor VulkanUtility::ToVulkanBlendFactor(BlendFactor blendFactor)
	{
		switch (blendFactor)
		{
		case BlendFactor::Zero:
			return VK_BLEND_FACTOR_ZERO;
		case BlendFactor::One:
			return VK_BLEND_FACTOR_ONE;
		case BlendFactor::SrcColor:
			return VK_BLEND_FACTOR_SRC_COLOR;
		case BlendFactor::InvSrcColor:
			return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
		case BlendFactor::DestColor:
			return VK_BLEND_FACTOR_DST_COLOR;
		case BlendFactor::InvDestColor:
			return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
		case BlendFactor::SrcAlpha:
			return VK_BLEND_FACTOR_SRC_ALPHA;
		case BlendFactor::InvSrcAlpha:
			return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		case BlendFactor::DestAlpha:
			return VK_BLEND_FACTOR_DST_ALPHA;
		case BlendFactor::InvDestAlpha:
			return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
		case BlendFactor::ConstColor:
			return VK_BLEND_FACTOR_CONSTANT_COLOR;
		case BlendFactor::InvConstColor:
			return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
		case BlendFactor::ConstAlpha:
			return VK_BLEND_FACTOR_CONSTANT_ALPHA;
		case BlendFactor::InvConstAlpha:
			return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
		case BlendFactor::SrcAlphaSaturate:
			return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
		case BlendFactor::Src1Color:
			return VK_BLEND_FACTOR_SRC1_COLOR;
		case BlendFactor::InvSrc1Color:
			return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
		case BlendFactor::Src1Alpha:
			return VK_BLEND_FACTOR_SRC1_ALPHA;
		case BlendFactor::InvSrc1Alpha:
			return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
		default:
			TYR_ASSERT(false);
			return VK_BLEND_FACTOR_MAX_ENUM;
		}
	}

	VkBlendOp VulkanUtility::ToVulkanBlendOp(BlendOp op)
	{
		switch (op)
		{
		case BlendOp::Add:
			return VK_BLEND_OP_ADD;
		case BlendOp::Subtract:
			return VK_BLEND_OP_SUBTRACT;
		case BlendOp::ReverseSubtract:
			return VK_BLEND_OP_REVERSE_SUBTRACT;
		case BlendOp::Min:
			return VK_BLEND_OP_MIN;
		case BlendOp::Max:
			return VK_BLEND_OP_MAX;
		default:
			TYR_ASSERT(false);
			return VK_BLEND_OP_MAX_ENUM;
		}
	}

	VkCompareOp VulkanUtility::ToVulkanCompareOp(CompareOp op)
	{
		switch (op)
		{
		case CompareOp::Never:
			return VK_COMPARE_OP_NEVER;
		case CompareOp::Less:
			return VK_COMPARE_OP_LESS;
		case CompareOp::Equal:
			return VK_COMPARE_OP_EQUAL;
		case CompareOp::LessOrEqual:
			return VK_COMPARE_OP_LESS_OR_EQUAL;
		case CompareOp::Greater:
			return VK_COMPARE_OP_GREATER;
		case CompareOp::NotEqual:
			return VK_COMPARE_OP_NOT_EQUAL;
		case CompareOp::GreaterOrEqual:
			return VK_COMPARE_OP_GREATER_OR_EQUAL;
		case CompareOp::Always:
			return VK_COMPARE_OP_ALWAYS;
		default:
			TYR_ASSERT(false);
			return VK_COMPARE_OP_MAX_ENUM;
		}
	}

	VkStencilOp VulkanUtility::ToVulkanStencilOp(StencilOp op)
	{
		switch (op)
		{
		case StencilOp::Keep:
			return VK_STENCIL_OP_KEEP;
		case StencilOp::Zero:
			return VK_STENCIL_OP_ZERO;
		case StencilOp::Replace:
			return VK_STENCIL_OP_REPLACE;
		case StencilOp::IncrementAndClamp:
			return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
		case StencilOp::DecrementAndClamp:
			return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
		case StencilOp::Invert:
			return VK_STENCIL_OP_INVERT;
		case StencilOp::IncrementAndWrap:
			return VK_STENCIL_OP_INCREMENT_AND_WRAP;
		case StencilOp::DecrementAndWrap:
			return VK_STENCIL_OP_DECREMENT_AND_WRAP;
		default:
			TYR_ASSERT(false);
			return VK_STENCIL_OP_MAX_ENUM;
		}
	}

	VkPipelineBindPoint VulkanUtility::ToVulkanPipelineBindPoint(PipelineType pipelineType)
	{
		switch (pipelineType)
		{
		case PipelineType::Graphics:
			return VK_PIPELINE_BIND_POINT_GRAPHICS;
		case PipelineType::Compute:
			return VK_PIPELINE_BIND_POINT_COMPUTE;
		case  PipelineType::RayTracing:
			return VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
		default:
			TYR_ASSERT(false);
			return VK_PIPELINE_BIND_POINT_MAX_ENUM;
		}
	}

	VkAttachmentLoadOp VulkanUtility::ToVulkanAttachmentLoadOp(AttachmentLoadOp op)
	{
		switch (op)
		{
		case AttachmentLoadOp::Load:
			return VK_ATTACHMENT_LOAD_OP_LOAD;
		case AttachmentLoadOp::Clear:
			return VK_ATTACHMENT_LOAD_OP_CLEAR;
		case AttachmentLoadOp::DontCare:
			return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		case AttachmentLoadOp::None:
			return VK_ATTACHMENT_LOAD_OP_NONE_EXT;
		default:
			TYR_ASSERT(false);
			return VK_ATTACHMENT_LOAD_OP_MAX_ENUM;
		}
	}

	VkAttachmentStoreOp VulkanUtility::ToVulkanAttachmentStoreOp(AttachmentStoreOp op)
	{
		switch (op)
		{
		case AttachmentStoreOp::Store: 
			return VK_ATTACHMENT_STORE_OP_STORE;
		case AttachmentStoreOp::DontCare: 
			return VK_ATTACHMENT_STORE_OP_DONT_CARE;
		case AttachmentStoreOp::None: 
			return VK_ATTACHMENT_STORE_OP_NONE_EXT;
		default:
			TYR_ASSERT(false);
			return VK_ATTACHMENT_STORE_OP_MAX_ENUM;
		}
	}

	VkDynamicState VulkanUtility::ToVulkanDynamicState(DynamicState dynamicState)
	{
		switch (dynamicState)
		{
		case DynamicState::Viewport:
			return VK_DYNAMIC_STATE_VIEWPORT;
		case DynamicState::Scissor:
			return VK_DYNAMIC_STATE_SCISSOR;
		case DynamicState::LineWidth:
			return VK_DYNAMIC_STATE_LINE_WIDTH;
		case DynamicState::DepthBias:
			return VK_DYNAMIC_STATE_DEPTH_BIAS;
		case DynamicState::BlendConstants:
			return VK_DYNAMIC_STATE_BLEND_CONSTANTS;
		case DynamicState::DepthBounds:
			return VK_DYNAMIC_STATE_DEPTH_BOUNDS;
		case DynamicState::StencilCompareMask:
			return VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK;
		case DynamicState::StencilWriteMask:
			return VK_DYNAMIC_STATE_STENCIL_WRITE_MASK;
		case DynamicState::StencilReference:
			return VK_DYNAMIC_STATE_STENCIL_REFERENCE;
		case DynamicState::CullMode:
			return VK_DYNAMIC_STATE_CULL_MODE;
		case DynamicState::FrontFace:
			return VK_DYNAMIC_STATE_FRONT_FACE;
		case DynamicState::PrimitiveTopology:
			return VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY;
		case DynamicState::ViewportWithCount:
			return VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT;
		case DynamicState::ScissorWithCount:
			return VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT;
		case DynamicState::VertexInputBindingStride:
			return VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE;
		case DynamicState::DepthTestEnable:
			return VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE;
		case DynamicState::DepthWriteEnable:
			return VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE;
		case DynamicState::DepthCompareOp:
			return VK_DYNAMIC_STATE_DEPTH_COMPARE_OP;
		case DynamicState::DepthBoundsTestEnable:
			return VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE;
		case DynamicState::StencilTestEnable:
			return VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE;
		case DynamicState::StencilOp:
			return VK_DYNAMIC_STATE_STENCIL_OP;
		case DynamicState::RaytracingPipelineStackSize:
			return VK_DYNAMIC_STATE_RAY_TRACING_PIPELINE_STACK_SIZE_KHR;
		default:
			TYR_ASSERT(false);
			return VK_DYNAMIC_STATE_MAX_ENUM;
		}
	}

	VkDescriptorType VulkanUtility::ToVulkanDescriptorType(DescriptorType descriptorType)
	{
		switch (descriptorType)
		{
		case DescriptorType::SamplerHandle:
			return VK_DESCRIPTOR_TYPE_SAMPLER;
		case DescriptorType::CombinedImageSampler:
			return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		case DescriptorType::SampledTwice:
			return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		case DescriptorType::StorageImage:
			return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		case DescriptorType::UniformTexelBuffer:
			return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
		case DescriptorType::StorageTexelBuffer:
			return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
		case DescriptorType::UniformBuffer:
			return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		case DescriptorType::StorageBuffer:
			return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		case DescriptorType::UniformBufferDynamic:
			return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		case DescriptorType::StorageBufferDynamic:
			return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
		case DescriptorType::InputAttachment:
			return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		case DescriptorType::AccelerationStructure:
			return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		default:
			TYR_ASSERT(false);
			return VK_DESCRIPTOR_TYPE_MAX_ENUM;
		}
	}

	VkVertexInputRate VulkanUtility::ToVulkanVertexInputRate(VertexInputRate inputRate)
	{
		switch (inputRate)
		{
		case VertexInputRate::Vertex:
			return VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX;
		case VertexInputRate::Instance:
			return VkVertexInputRate::VK_VERTEX_INPUT_RATE_INSTANCE;
		default:
			TYR_ASSERT(false);
			return VK_VERTEX_INPUT_RATE_MAX_ENUM;
		}
	}

	VkPolygonMode VulkanUtility::ToVulkanPolygonMode(PolygonMode mode)
	{
		switch (mode)
		{
		case PolygonMode::Fill:
			return VK_POLYGON_MODE_FILL;
		case PolygonMode::Line:
			return VK_POLYGON_MODE_LINE;
		case PolygonMode::Point:
			return VK_POLYGON_MODE_POINT;
		default:
			TYR_ASSERT(false);
			return VK_POLYGON_MODE_MAX_ENUM;
		}
	}

	VkFrontFace VulkanUtility::ToVulkanFrontFace(FrontFace face)
	{
		switch (face)
		{
		case FrontFace::CounterClockwise:
			return VK_FRONT_FACE_COUNTER_CLOCKWISE;
		case FrontFace::Clockwise:
			return VK_FRONT_FACE_CLOCKWISE;
		default:
			TYR_ASSERT(false);
			return VK_FRONT_FACE_MAX_ENUM;
		}
	}

	VkSharingMode VulkanUtility::ToVulkanSharingMode(SharingMode mode)
	{
		switch (mode)
		{
		case SharingMode::Exclusive:
			return VK_SHARING_MODE_EXCLUSIVE;
		case SharingMode::Concurrent:
			return VK_SHARING_MODE_CONCURRENT;
		default:
			TYR_ASSERT(false);
			return VK_SHARING_MODE_MAX_ENUM;
		}
	}

	VkCommandBufferLevel VulkanUtility::ToVulkanCommandBufferLevel(CommandListType type)
	{
		switch (type)
		{
		case CommandListType::Primary:
			return VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		case CommandListType::Secondary:
			return VK_COMMAND_BUFFER_LEVEL_SECONDARY;
		default:
			TYR_ASSERT(false);
			return VK_COMMAND_BUFFER_LEVEL_MAX_ENUM;
		}
	}

	VkFilter VulkanUtility::ToVulkanFilter(Filter filter)
	{
		switch (filter)
		{
		case Filter::Nearest:
			return VK_FILTER_NEAREST;
		case Filter::Linear:
			return VK_FILTER_LINEAR;
		default:
			TYR_ASSERT(false);
			return VK_FILTER_MAX_ENUM;
		}
	}

	VkSamplerMipmapMode VulkanUtility::ToVulkanSamplerMipmapMode(SamplerMipmapMode mode)
	{
		switch (mode)
		{
		case SamplerMipmapMode::Nearest:
			return VK_SAMPLER_MIPMAP_MODE_NEAREST;
		case SamplerMipmapMode::Linear:
			return VK_SAMPLER_MIPMAP_MODE_LINEAR;
		default:
			TYR_ASSERT(false);
			return VK_SAMPLER_MIPMAP_MODE_MAX_ENUM;
		}
	}

	VkSamplerAddressMode VulkanUtility::ToVulkanSamplerAddressMode(SamplerAddressMode mode)
	{
		switch (mode)
		{
		case SamplerAddressMode::Repeat:
			return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		case SamplerAddressMode::MirroredRepeat:
			return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		case SamplerAddressMode::ClampToEdge:
			return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		case SamplerAddressMode::ClampToBorder:
			return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		case SamplerAddressMode::MirrorClampToEdge:
			return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
		default:
			TYR_ASSERT(false);
			return VK_SAMPLER_ADDRESS_MODE_MAX_ENUM;
		}
	}

	VkBorderColor VulkanUtility::ToVulkanBorderColour(BorderColour colour)
	{
		switch (colour)
		{
		case BorderColour::FloatTransparentBlack:
			return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
		case BorderColour::IntTransparentBlack:
			return VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
		case BorderColour::FloatOpaqueBlack:
			return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
		case BorderColour::IntOpaqueBlack:
			return VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		case BorderColour::FloatOpaqueWhite:
			return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		case BorderColour::IntOpaqueWhite:
			return VK_BORDER_COLOR_INT_OPAQUE_WHITE;
		default:
			TYR_ASSERT(false);
			return VK_BORDER_COLOR_MAX_ENUM;
		}
	}
}

