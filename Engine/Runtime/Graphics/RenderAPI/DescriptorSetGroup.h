
#pragma once

#include "GraphicsBase.h"
#include "RenderAPITypes.h"
#include "Buffer.h"
#include "Image.h"

namespace tyr
{
	using DescriptorPoolHandle = ResourceHandle;
	using DescriptorSetLayoutHandle = ResourceHandle;
	using DescriptorSetGroupHandle = ResourceHandle;

	struct DescriptorPoolSize
	{
		DescriptorType descriptorType;
		uint descriptorCount;
	};

	struct DescriptorPoolDesc
	{
#if !TYR_FINAL
		GDebugString debugName;
#endif
		LocalArray<DescriptorPoolSize, 8> poolSizes;
		uint maxSets;
	};

	struct DescriptorSetLayoutBinding
	{
		uint binding;
		DescriptorType descriptorType;
		uint descriptorCount;
		ShaderStage stageFlags;
	};

	static constexpr uint c_MaxDescriptorBindings = 10u;
	struct DescriptorSetLayoutDesc
	{
#if !TYR_FINAL
		GDebugString debugName;
#endif
		DescriptorSetLayoutFlags flags;
		LocalArray<DescriptorSetLayoutBinding, c_MaxDescriptorBindings> bindings;
	};

	static constexpr uint c_MaxDescriptorSetsPerGroup = 8;
	// a DescriptorsetGroup is a group of descriptor sets in vulkan and a route signature in D3D12
	struct DescriptorSetGroupDesc
	{
		
#if !TYR_FINAL
		GDebugString debugName;
#endif
		DescriptorPoolHandle pool;
		LocalArray<DescriptorSetLayoutHandle, c_MaxDescriptorSetsPerGroup> layouts;
	};

	struct BindingUpdate
	{
		uint descriptorIndex;
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
		LocalArray<BufferBindingInfo, 10> bufferBindingInfos;
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
		LocalArray<ImageBindingInfo, 10> imageBindingInfos;
	};

	// TODO: Support below
	struct AccelerationStructureBindingUpdate : BindingUpdate
	{
		int placeHolder;
	};
}
