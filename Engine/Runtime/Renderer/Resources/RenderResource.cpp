#include "RenderResource.h"
#include "RenderAPI/Device.h"

namespace tyr
{
	std::atomic<ResourceID> RenderResource::s_NextID = 0u;

	RenderResource::RenderResource(Device& device)
		: m_Device(device)
		, m_ID(s_NextID++)
		, m_AccessState(BARRIER_ACCESS_NONE)
	{
		
	}

	RenderResource::~RenderResource()
	{

	}
}