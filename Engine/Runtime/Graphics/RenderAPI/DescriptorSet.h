
#pragma once

#include "GraphicsBase.h"
#include "RenderAPITypes.h"

namespace tyr
{
	class BufferView;
	class ImageView;
	class Sampler;

	struct DescriptorPoolSize
	{
		DescriptorType descriptorType;
		uint descriptorCount;
	};

	struct DescriptorPoolDesc
	{
		Array<DescriptorPoolSize> poolSizes;
		uint maxSets;
	};

	/// Class repesenting a descriptor pool. 
	class TYR_GRAPHICS_EXPORT DescriptorPool
	{
	public:
		DescriptorPool(DescriptorPoolDesc desc);
		virtual ~DescriptorPool();

		const DescriptorPoolDesc& GetDesc() const { return m_Desc; }
	protected:
		DescriptorPoolDesc m_Desc;
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
		DescriptorSetLayoutFlags flags;
		uint bindingCount = 0;
		// Should be ordered by type and then binding number.
		DescriptorSetLayoutBinding bindings[c_MaxDescriptorBindings];
	};

	/// Class repesenting a descriptor set layout. 
	class TYR_GRAPHICS_EXPORT DescriptorSetLayout
	{
	public:
		DescriptorSetLayout(const DescriptorSetLayoutDesc& desc);
		virtual ~DescriptorSetLayout();

		const DescriptorSetLayoutDesc& GetDesc() const { return m_Desc; }
	protected:
		DescriptorSetLayoutDesc m_Desc;
	};

	static constexpr uint c_MaxDescriptorSetsPerGroup = 20u;
	struct DescriptorSetGroupDesc
	{
		Ref<DescriptorPool> pool;
		uint layoutCount = 0;
		// There should be one layout per set
		// The desc will be cached and therefore any layouts passed should not be freed before the descriptor set group.
		Ref<DescriptorSetLayout> layouts[c_MaxDescriptorSetsPerGroup];
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
		Ref<BufferView> bufferView;
	};

	struct BufferBindingUpdate : BindingUpdate
	{
		Array<BufferBindingInfo> bufferBindingInfos;
	};

	// Can be multiple per binding slot (ie. array of images)
	struct ImageBindingInfo
	{
		Ref<ImageView> imageView;
		Ref<Sampler> sampler;
	};

	struct ImageBindingUpdate : BindingUpdate
	{
		Array<ImageBindingInfo> imageBindingInfos;
	};

	// TODO: Support below
	struct AccelerationStructureBindingUpdate : BindingUpdate
	{
		int placeHolder;
	};

	/// Class repesenting a group of descriptor sets. 
	/// Grouping allows allocating and updating descriptor sets together for better performance 
	/// with certain APIs such as Vulkan. Should group per pipeline and duplicate groups for variation in bindings / parallelism.
	class TYR_GRAPHICS_EXPORT DescriptorSetGroup
	{
	public:
		DescriptorSetGroup(const DescriptorSetGroupDesc& desc);
		virtual ~DescriptorSetGroup();

		virtual void Update(const BufferBindingUpdate* bufferUpdates, uint bufferUpdateCount, const ImageBindingUpdate* imageUpdates = nullptr,
			uint imageUpdateCount = 0, const AccelerationStructureBindingUpdate* accelerationStructureUpdates = nullptr,
			uint accelerationStructureUpdateCount = 0) = 0;

		const DescriptorSetGroupDesc& GetDesc() const { return m_Desc; }
	protected:
		DescriptorSetGroupDesc m_Desc;
	};
}
