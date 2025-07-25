#pragma once

#include "GraphicsBase.h"
#include "Math/Vector2I.h"

namespace tyr
{
	// Note: Some values in the below enums may be supported in some graphics APIs and not others.
	// The types are most familiar to the Vulkan API.

	#define QUEUE_FAMILY_IGNORED (~0U)

	enum CommandQueueType
	{
		CQ_GRAPHICS = 0,
		CQ_COMPUTE = 1,
		CQ_TRANSFER = 2,
		CQ_COUNT = 3
	};

	enum PixelFormat : uint8
	{
		PF_UNKNOWN = 0,
		PF_R8G8B8A8_UNORM,
		PF_R8G8B8A8_SNORM,
		PF_R8G8B8A8_SSCALED,
		PF_R8G8B8A8_USCALED,
		PF_R8G8B8A8_SINT,
		PF_R8G8B8A8_UINT,	
		PF_R8G8B8A8_SRGB,
		
		PF_R16G16B16A16_UNORM,
		PF_R16G16B16A16_SNORM,
		PF_R16G16B16A16_SSCALED,
		PF_R16G16B16A16_USCALED,
		PF_R16G16B16A16_SINT,
		PF_R16G16B16A16_UINT,
		PF_R16G16B16A16_SFLOAT,

		PF_R32G32B32_UINT,
		PF_R32G32B32_SINT,
		PF_R32G32B32_SFLOAT,
		PF_R32G32B32A32_SINT,
		PF_R32G32B32A32_UINT,
		PF_R32G32B32A32_SFLOAT,

		PF_B8G8R8A8_UNORM,
		PF_B8G8R8A8_SNORM,
		PF_B8G8R8A8_SSCALED,
		PF_B8G8R8A8_USCALED,
		PF_B8G8R8A8_SINT,
		PF_B8G8R8A8_UINT,
		PF_B8G8R8A8_SRGB,

		PF_B16G16R16A16_UNORM,
		PF_B16G16R16A16_SNORM,
		PF_B16G16R16A16_SSCALED,
		PF_B16G16R16A16_USCALED,
		PF_B16G16R16A16_SINT,
		PF_B16G16R16A16_UINT,
		PF_B16G16R16A16_SFLOAT,

		// Depth formats:
		PF_D16_UNORM,
		PF_D24_UNORM_S8_UINT,
		PF_D32_SFLOAT,
		PF_D32_FLOAT_S8_UINT,

		// Compression formats
		PF_BC3_UNORM,
		PF_BC3_SRGB,
		PF_BC5_UNORM,
		PF_BC5_SNORM,
		PF_BC7_UNORM,
		PF_BC7_SRGB,

		PF_COUNT
	};

	enum ColorSpace
	{
		CP_SRGB_NONLINEAR = 0,
		CP_BT709_LINEAR,
		CP_BT709_NONLINEAR,
		CP_BT2020_LINEAR,
	};

	enum ColorComponent : uint8
	{
		COLOR_COMPONENT_R_BIT = 0x00000001,
		COLOR_COMPONENT_G_BIT = 0x00000002,
		COLOR_COMPONENT_B_BIT = 0x00000004,
		COLOR_COMPONENT_A_BIT = 0x00000008
	};

	enum ImageLayout : uint8
	{
		IMAGE_LAYOUT_UNKNOWN = 0,
		IMAGE_LAYOUT_GENERAL,
		IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		IMAGE_LAYOUT_PREINITIALISED,
		IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL,
		IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL,
		IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
		IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
		IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL,
		IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL,
		IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
		IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
		IMAGE_LAYOUT_PRESENT_SRC,
		IMAGE_LAYOUT_SHARED_PRESENT,
		IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL

		// D3D12
		/*IMAGE_LAYOUT_RESOLVE_SOURCE,
		IMAGE_LAYOUT_RESOLVE_DEST,
		IMAGE_LAYOUT_UNORDERED_ACCESS,
		IMAGE_LAYOUT_NON_PIXEL_SHADER_READ_ONLY_OPTIMAL*/
	};

	enum ResolveMode 
	{
		RESOLVE_MODE_NONE = 0,
		RESOLVE_MODE_SAMPLE_ZERO_BIT = 0x00000001,
		RESOLVE_MODE_AVERAGE_BIT = 0x00000002,
		RESOLVE_MODE_MIN_BIT = 0x00000004,
		RESOLVE_MODE_MAX_BIT = 0x00000008
	};

	enum PipelineStage : uint64
	{
		PIPELINE_STAGE_NONE = 0,
		PIPELINE_STAGE_TOP_OF_PIPE_BIT = 0x00000001,
		PIPELINE_STAGE_DRAW_INDIRECT_BIT = 0x00000002,
		PIPELINE_STAGE_VERTEX_INPUT_BIT = 0x00000004,
		PIPELINE_STAGE_VERTEX_SHADER_BIT = 0x00000008,
		PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT = 0x00000010,
		PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT = 0x00000020,
		PIPELINE_STAGE_GEOMETRY_SHADER_BIT = 0x00000040,
		PIPELINE_STAGE_FRAGMENT_SHADER_BIT = 0x00000080,
		PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT = 0x00000100,
		PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT = 0x00000200,
		PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT = 0x00000400,
		PIPELINE_STAGE_COMPUTE_SHADER_BIT = 0x00000800,
		PIPELINE_STAGE_TRANSFER_BIT = 0x00001000,
		PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT = 0x00002000,
		PIPELINE_STAGE_HOST_BIT = 0x00004000,
		PIPELINE_STAGE_ALL_GRAPHICS_BIT = 0x00008000,
		PIPELINE_STAGE_ALL_COMMANDS_BIT = 0x00010000,
		PIPELINE_STAGE_CLEAR_BIT = 0x800000000,
		

		// Extensions
		PIPELINE_STAGE_TASK_SHADER_BIT = 0x00080000,
		PIPELINE_STAGE_MESH_SHADER_BIT = 0x00100000,
		PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT = 0x01000000,
		PIPELINE_STAGE_CONDITIONAL_RENDERING_BIT = 0x00040000,
		PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT = 0x02000000,
		PIPELINE_STAGE_RAY_TRACING_SHADER_BIT = 0x00200000,
		PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT = 0x00800000,
		PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT = 0x00400000
	};

	enum BarrierAccess : uint64
	{
		BARRIER_ACCESS_NONE = 0,
		BARRIER_ACCESS_INDIRECT_COMBAND_READ_BIT = 0x00000001,
		BARRIER_ACCESS_INDEX_READ_BIT = 0x00000002,
		BARRIER_ACCESS_VERTEX_ATTRIBUTE_READ_BIT = 0x00000004,
		BARRIER_ACCESS_UNIFORM_READ_BIT = 0x00000008,
		BARRIER_ACCESS_INPUT_ATTACHMENT_READ_BIT = 0x00000010,
		BARRIER_ACCESS_SHADER_READ_BIT = 0x00000020,
		BARRIER_ACCESS_SHADER_WRITE_BIT = 0x00000040,
		BARRIER_ACCESS_COLOR_ATTACHMENT_READ_BIT = 0x00000080,
		BARRIER_ACCESS_COLOR_ATTACHMENT_WRITE_BIT = 0x00000100,
		BARRIER_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT = 0x00000200,
		BARRIER_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT = 0x00000400,
		BARRIER_ACCESS_TRANSFER_READ_BIT = 0x00000800,
		BARRIER_ACCESS_TRANSFER_WRITE_BIT = 0x00001000,
		BARRIER_ACCESS_HOST_READ_BIT = 0x00002000,
		BARRIER_ACCESS_HOST_WRITE_BIT = 0x00004000,
		BARRIER_ACCESS_MEMORY_READ_BIT = 0x00008000,
		BARRIER_ACCESS_MEMORY_WRITE_BIT = 0x00010000,

		// Extensions
		BARRIER_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT = 0x02000000,
		BARRIER_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT = 0x04000000,
		BARRIER_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT = 0x08000000,
		BARRIER_ACCESS_CONDITIONAL_RENDERING_READ_BIT = 0x00100000,
		BARRIER_ACCESS_ACCELERATION_STRUCTURE_READ_BIT = 0x00200000,
		BARRIER_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT = 0x00400000,
		BARRIER_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT = 0x01000000,
		BARRIER_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT = 0x00800000
	};

	enum DescriptorSetLayoutFlags
	{
		DESCRIPTOR_SET_LAYOUT_NONE = 0,
		DESCRIPTOR_SET_LAYOUT_PUSH_DESCRIPTOR_BIT = 0x00000001,
		DESCRIPTOR_SET_LAYOUT_UPDATE_AFTER_BIND_POOL_BIT = 0x00000002,
		DESCRIPTOR_SET_LAYOUT_HOST_ONLY_POOL_BIT = 0x00000004
	};

	enum ShaderStage 
	{
		SHADER_STAGE_VERTEX_BIT = 0x00000001,
		SHADER_STAGE_TESSELLATION_CONTROL_BIT = 0x00000002,
		SHADER_STAGE_TESSELLATION_EVALUATION_BIT = 0x00000004,
		SHADER_STAGE_GEOMETRY_BIT = 0x00000008,
		SHADER_STAGE_FRAGMENT_BIT = 0x00000010,
		SHADER_STAGE_COMPUTE_BIT = 0x00000020,
		SHADER_STAGE_ALL_GRAPHICS = 0x0000001F,
		SHADER_STAGE_ALL = 0x7FFFFFFF,
		SHADER_STAGE_RAYGEN_BIT = 0x00000100,
		SHADER_STAGE_ANY_HIT_BIT = 0x00000200,
		SHADER_STAGE_CLOSEST_HIT_BIT = 0x00000400,
		SHADER_STAGE_MISS_BIT = 0x00000800,
		SHADER_STAGE_INTERSECTION_BIT = 0x00001000,
		SHADER_STAGE_CALLABLE_BIT = 0x00002000
	};

	enum Dependency
	{
		DEPENDENCY_UNKNOWN = 0,
		DEPENDENCY_BY_REGION_BIT = 0x00000001,
		DEPENDENCY_VIEW_LOCAL_BIT = 0x00000002,
		DEPENDENCY_DEVICE_GROUP_BIT = 0x00000004,
		DEPENDENCY_FEEDBACK_LOOP_BIT = 0x00000008
	};

	enum BufferUsage : uint
	{
		BUFFER_USAGE_UNKNOWN = 0,
		BUFFER_USAGE_TRANSFER_SRC_BIT = 0x00000001,
		BUFFER_USAGE_TRANSFER_DST_BIT = 0x00000002,
		BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT = 0x00000004,
		BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT = 0x00000008,
		BUFFER_USAGE_UNIFORM_BUFFER_BIT = 0x00000010,
		BUFFER_USAGE_STORAGE_BUFFER_BIT = 0x00000020,
		BUFFER_USAGE_INDEX_BUFFER_BIT = 0x00000040,
		BUFFER_USAGE_VERTEX_BUFFER_BIT = 0x00000080,
		BUFFER_USAGE_INDIRECT_BUFFER_BIT = 0x00000100,
		BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT = 0x00020000,
		BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT = 0x00000800,
		BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT = 0x00001000,
		BUFFER_USAGE_CONDITIONAL_RENDERING_BIT = 0x00000200,
		BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT = 0x00080000,
		BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT = 0x00100000,
		BUFFER_USAGE_SHADER_BINDING_TABLE_BIT = 0x00000400,
		BUFFER_USAGE_RAY_TRACING_BIT = BUFFER_USAGE_SHADER_BINDING_TABLE_BIT
	};

	enum ImageUsage 
	{
		IMAGE_USAGE_TRANSFER_SRC_BIT = 0x00000001,
		IMAGE_USAGE_TRANSFER_DST_BIT = 0x00000002,
		IMAGE_USAGE_SAMPLED_BIT = 0x00000004,
		IMAGE_USAGE_STORAGE_BIT = 0x00000008,
		IMAGE_USAGE_COLOUR_ATTACHMENT_BIT = 0x00000010,
		IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT = 0x00000020,
		IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT = 0x00000040,
		IMAGE_USAGE_INPUT_ATTACHMENT_BIT = 0x00000080
	};

	enum SubresourceAspect 
	{
		SUBRESOURCE_ASPECT_NONE = 0,
		SUBRESOURCE_ASPECT_COLOUR_BIT = 0x00000001,
		SUBRESOURCE_ASPECT_DEPTH_BIT = 0x00000002,
		SUBRESOURCE_ASPECT_STENCIL_BIT = 0x00000004,
		SUBRESOURCE_ASPECT_METADATA_BIT = 0x00000008,
		SUBRESOURCE_ASPECT_PLANE_0_BIT = 0x00000010,
		SUBRESOURCE_ASPECT_PLANE_1_BIT = 0x00000020,
		SUBRESOURCE_ASPECT_PLANE_2_BIT = 0x00000040
	};

	enum MemoryProperty 
	{
		MEMORY_PROPERTY_DEVICE_LOCAL_BIT = 0x00000001,
		MEMORY_PROPERTY_HOST_VISIBLE_BIT = 0x00000002,
		MEMORY_PROPERTY_HOST_COHERENT_BIT = 0x00000004,
		MEMORY_PROPERTY_HOST_CACHED_BIT = 0x00000008,
		MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT = 0x00000010,
		MEMORY_PROPERTY_PROTECTED_BIT = 0x00000020
	};

	enum CommandAllocatorCreateFlags 
	{
		COMMAND_ALLOC_CREATE_TRANSIENT_BIT = 0x00000001,
		COMMAND_ALLOC_CREATE_RESET_COMMAND_BUFFER_BIT = 0x00000002,
		COMMAND_ALLOC_CREATE_PROTECTED_BIT = 0x00000004
	};

	enum CommandBufferUsage
	{
		COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT = 0x00000001,
		COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT = 0x00000002,
		COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT = 0x00000004
	};

	enum RenderingInfoFlags
	{
		RENDERING_INFO_NONE = 0,
		RENDERING_INFO_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT = 0x00000001,
		RENDERING_INFP_SUSPENDING_BIT = 0x00000002,
		RENDERING_INFO_RESUMING_BIT = 0x00000004,
		RENDERING_INFO_ENABLE_LEGACY_DITHERING_BIT_EXT = 0x00000008,
	};

	enum class PipelineType 
	{
		Graphics = 0,
		Compute,
		RayTracing
	};

	enum class AttachmentLoadOp 
	{
		Load,
		Clear,
		DontCare,
		None
	};

	enum class AttachmentStoreOp 
	{
		Store,
		DontCare,
		None
	};

	enum class SampleCount : uint8
	{
		OneBit,
		TwoBit,
		FourBit,
		EightBit,
		SixteenBit,
		ThirtyTwoBit,
		SixtyFourBit
	};

	enum class PrimitiveTopology
	{
		PointList,
		LineList,
		LineStrip,
		TriangeList,
		TriangleStrip
	};

	enum class CullMode : uint8
	{
		None,
		Front,
		Back,
		FrontAndBack
	};

	enum class BlendFactor
	{
		Zero,
		One,
		SrcColor,
		InvSrcColor,
		SrcAlpha,
		InvSrcAlpha,
		DestColor,
		InvDestColor,
		DestAlpha,
		InvDestAlpha,
		ConstColor,
		InvConstColor,
		ConstAlpha,
		InvConstAlpha,
		SrcAlphaSaturate,
		Src1Color,
		InvSrc1Color,
		Src1Alpha,
		InvSrc1Alpha
	};

	// Vulkan supports a lot more values while D3D12 supports only these. Add to this later if needed.
	enum class BlendOp : uint
	{
		Add = 0,
		Subtract,
		ReverseSubtract,
		Min,
		Max	
	};

	enum class CompareOp : uint
	{
		Never = 0,
		Less,
		Equal,
		LessOrEqual,
		Greater,
		NotEqual,
		GreaterOrEqual,
		Always
	};

	enum class StencilOp : uint
	{
		Keep = 0,
		Zero,
		Replace,
		IncrementAndClamp,
		DecrementAndClamp,
		Invert,
		IncrementAndWrap,
		DecrementAndWrap
	};

	enum class DynamicState : uint
	{
		Viewport = 0,
		Scissor,
		LineWidth,
		DepthBias,
		BlendConstants,
		DepthBounds,
		StencilCompareMask,
		StencilWriteMask,
		StencilReference,
		CullMode,
		FrontFace,
		PrimitiveTopology,
		ViewportWithCount,
		ScissorWithCount,
		VertexInputBindingStride,
		DepthTestEnable,
		DepthWriteEnable,
		DepthCompareOp,
		DepthBoundsTestEnable,
		StencilTestEnable,
		StencilOp,
		RaytracingPipelineStackSize
	};

	enum class DescriptorType : uint
	{
		Sampler = 0,
		CombinedImageSampler,
		SampledTwice,
		StorageImage,
		UniformTexelBuffer,
		StorageTexelBuffer,
		UniformBuffer,
		StorageBuffer,
		UniformBufferDynamic,
		StorageBufferDynamic,
		InputAttachment,
		AccelerationStructure,

		// D3D12 only
		RenderTargetView,
		DepthStencilView
	};

	enum class VertexInputRate : uint8
	{
		Vertex = 0,
		Instance
	};

	enum class PolygonMode : uint8
	{
		Fill = 0,
		Line,
		Point
	};

	enum class FrontFace : uint8
	{
		CounterClockwise = 0,
		Clockwise
	};

	enum class SharingMode 
	{
		Exclusive = 0,
		Concurrent
	};

	enum class CommandListType
	{
		Primary,
		Secondary
	};

	enum class SemaphoreType
	{
		Binary,
		Timeline
	};

	enum class ImageType : uint8
	{
		Image1D = 0,
		Image2D,
		Image3D,
		Cubemap,
		Image1DArray,
		Image2DArray,
		CubemapArray
	};

	enum class Filter : uint
	{
		Nearest,
		Linear
	};

	enum class SamplerMipmapMode : uint
	{
		Nearest,
		Linear
	};

	enum class SamplerAddressMode : uint
	{
		Repeat = 0,
		MirroredRepeat,
		ClampToEdge,
		ClampToBorder,
		MirrorClampToEdge
	};

	enum class BorderColour : uint
	{
		FloatTransparentBlack = 0,
		IntTransparentBlack,
		FloatOpaqueBlack,
		IntOpaqueBlack,
		FloatOpaqueWhite,
		IntOpaqueWhite
	};

	struct SceneViewArea
	{
		// Values are in NDC space with min 0 and max 1
		float x = 0.0f;
		float y = 0.0f;
		float width = 1.0f;
		float height = 1.0f;
	};

	struct Viewport
	{
		// x and y values are in NDC space with min 0 and max 1
		float x = 0.0f;
		float y = 0.0f;
		float width = 1920;
		float height = 1080;
		float minDepth = 0.0f;
		float maxDepth = 1.0f;
	};

	struct GraphicsRect
	{
		Vector2I offset;
		Extents2 extents;
	};
}


