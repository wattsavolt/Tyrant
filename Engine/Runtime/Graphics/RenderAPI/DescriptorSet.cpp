
#include "DescriptorSet.h"

namespace tyr
{
	DescriptorPool::DescriptorPool(DescriptorPoolDesc desc)
		: m_Desc(desc)
	{

	}

	DescriptorPool::~DescriptorPool()
	{

	}

	DescriptorSetLayout::DescriptorSetLayout(const DescriptorSetLayoutDesc& desc)
		: m_Desc(desc)
	{
		TYR_ASSERT(desc.bindingCount <= c_MaxDescriptorBindings);
	}

	DescriptorSetLayout::~DescriptorSetLayout()
	{

	}

	DescriptorSetGroup::DescriptorSetGroup(const DescriptorSetGroupDesc& desc)
		: m_Desc(desc)
	{
		TYR_ASSERT(desc.layoutCount <= c_MaxDescriptorSetsPerGroup);
	}

	DescriptorSetGroup::~DescriptorSetGroup()
	{

	}
}