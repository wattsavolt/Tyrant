
#include "RenderAPI.h"
#include "Device.h"
#include "SwapChain.h"

namespace tyr
{
#if TYR_DEBUG == 1
	const bool RenderAPI::m_ValidationLayersEnabled = true;
#else
	const bool RenderAPI::m_ValidationLayersEnabled = false;
#endif

	RenderAPI::RenderAPI()
		: m_Initialized(false)
		, m_Device(nullptr)
		, m_SwapChain(nullptr)
	{
		
	}

	RenderAPI::~RenderAPI()
	{
		TYR_ASSERT(!m_Initialized)
	}

	void RenderAPI::Initialize(const RenderAPIConfig& config)
	{
		m_Config = config;
		InitializeAPI();	
		m_Initialized = true;
	}

	void RenderAPI::Shutdown()
	{
		ShutdownAPI();
		m_Initialized = false;
	}
	
}