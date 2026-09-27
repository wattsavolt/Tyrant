
#pragma once

#include "GraphicsBase.h"
#include "RenderAPITypes.h"
#include "Buffer.h"
#include "Image.h"
#include "AccelerationStructure.h"

namespace tyr
{
	TYR_CREATE_HANDLE_TYPE(DescriptorPoolHandle);
	TYR_CREATE_HANDLE_TYPE(DescriptorSetLayoutHandle);
	TYR_CREATE_HANDLE_TYPE(DescriptorSetHandle);

	struct DescriptorPoolSize
	{
		DescriptorType descriptorType;
		uint descriptorCount;
	};

	struct DescriptorPoolDesc
	{
		TYR_DECLARE_GDEBUGNAME(debugName);
		LocalArray<DescriptorPoolSize, 8> poolSizes;
		uint maxSets;
		DescriptorPoolFlags flags;
	};

	struct DescriptorSetLayoutBinding
	{
		uint binding;
		DescriptorType descriptorType;
		uint descriptorCount;
		ShaderStage stageFlags;
		DescriptorBindingFlags bindingFlags;
	};

	static constexpr uint c_MaxDescriptorBindings = 32u;
	struct DescriptorSetLayoutDesc
	{
		TYR_DECLARE_GDEBUGNAME(debugName);
		DescriptorSetLayoutFlags flags;
		LocalArray<DescriptorSetLayoutBinding, c_MaxDescriptorBindings> bindings;
	};

	struct DescriptorSetDesc
	{
		TYR_DECLARE_GDEBUGNAME(debugName);
		DescriptorPoolHandle pool;
		DescriptorSetLayoutHandle layout;
	};

	struct BindingUpdate
	{
		uint bindingIndex;
		uint descriptorArrayIndex = 0;
	};

	// Can be multiple per binding slot (ie. array of buffers)
	struct BufferBindingInfo
	{
		BufferViewHandle bufferView;
	};

	struct BufferBindingUpdate : BindingUpdate
	{
		const BufferBindingInfo* bufferBindingInfos;
		uint infoCount = 0;
	};

	// Can be multiple per binding slot (ie. array of images)
	struct ImageBindingInfo
	{
		ImageViewHandle imageView;
		SamplerHandle sampler;
		bool hasSampler = true;
		// The layout the image is expected to be in whenever this descriptor is actually read
		// in a shader (not necessarily its layout right now, at write time) - a sampled/storage
		// image descriptor needs a real layout here, never UNDEFINED, or the read is invalid
		// usage, and it must match whatever layout the image is actually kept in (see
		// RendererAPI::CreateTexture, which always sets both to GENERAL). Meaningless (left
		// default) for a pure-sampler binding, which has no image at all - see this binding
		// type's own handling at the call site.
		ImageLayout layout = ImageLayout::IMAGE_LAYOUT_GENERAL;
	};

	struct ImageBindingUpdate : BindingUpdate
	{
		const ImageBindingInfo* imageBindingInfos;
		uint infoCount = 0;
	};

	// Can be multiple per binding slot (ie. array of top-level acceleration structures)
	struct AccelerationStructureBindingInfo
	{
		AccelerationStructureHandle accelerationStructure;
	};

	struct AccelerationStructureBindingUpdate : BindingUpdate
	{
		const AccelerationStructureBindingInfo* accelerationStructureBindingInfos;
		uint infoCount = 0;
	};
}
