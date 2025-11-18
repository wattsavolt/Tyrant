#pragma once

#include "Core.h"
#include "RendererMacros.h"
#include "Resources/RenderBuffer.h"

namespace tyr
{
	struct ResourceManagerConfig
	{
		uint maxTransferBuffers = 100u;
	};

	class ResourceManager final : INonCopyable
	{
	public:
		static ResourceManager* Get();
	
	
	private:
		friend class Renderer;

		ResourceManager(const ResourceManagerConfig& config);
		~ResourceManager();

		ResourceManagerConfig m_Config;
		Array<RenderBuffer*> m_TransferBuffers;
		
		
		static ResourceManager* s_Instance;
	};
	
}