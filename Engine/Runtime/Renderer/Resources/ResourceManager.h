#pragma once

#include "Core.h"
#include "RendererMacros.h"
#include "Resources/TransferBuffer.h"

namespace tyr
{
	class Buffer;
	class Buffer;
	class Image;

	struct ResourceManagerConfig
	{
		uint transferBufferReserveCount = 1000u;
	};

	class ResourceManager final : INonCopyable
	{
	public:
		static ResourceManager* Get();
	
	
	private:
		friend class SceneRenderer;

		ResourceManager(const ResourceManagerConfig& config);
		~ResourceManager();

		ResourceManagerConfig m_Config;
		Array<Ref<TransferBuffer>> m_TransferBuffers;
		
		
		static ResourceManager* s_Instance;
	};
	
}