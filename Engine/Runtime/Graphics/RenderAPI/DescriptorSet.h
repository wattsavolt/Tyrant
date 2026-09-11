
#pragma once

#include "GraphicsBase.h"
#include "RenderAPITypes.h"
#include "Buffer.h"
#include "Image.h"

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

	static constexpr uint c_MaxDescriptorBindings = 16u;
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
	};

	struct ImageBindingUpdate : BindingUpdate
	{
		const ImageBindingInfo* imageBindingInfos;
		uint infoCount = 0;
	};

	// TODO: Support below
	struct AccelerationStructureBindingUpdate : BindingUpdate
	{
		int placeHolder;
	};
}
