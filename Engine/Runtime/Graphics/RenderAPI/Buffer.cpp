#include "Buffer.h"

namespace tyr
{
	Buffer::Buffer(const BufferDesc& desc)
		: m_Desc(desc)
		, m_SizeAllocated(0)
	{
		
	}

	BufferView::BufferView(const BufferViewDesc& desc)
		: m_Desc(desc)
	{

	}
}