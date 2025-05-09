
#include "Image.h"
#include "Buffer.h"
#include "Device.h"

namespace tyr
{
	Image::Image(Device& device, const ImageDesc& desc)
		: m_Device(device)
		, m_Desc(desc)
		, m_SizeAllocated(0)
	{
		
	}

	ImageView::ImageView(const ImageViewDesc& desc)
		: m_Desc(desc)
	{

	}

	Sampler::Sampler(const SamplerDesc& desc)
		: m_Desc(desc)
	{

	}
}