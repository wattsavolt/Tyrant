#include "ResourceManager.h"
#include "RenderAPI/RenderAPI.h"
#include "RenderAPI/Buffer.h"
#include "RenderAPI/Buffer.h"
#include "RenderAPI/Image.h"

namespace tyr
{
	ResourceManager* ResourceManager::s_Instance = nullptr;

	ResourceManager::ResourceManager(const ResourceManagerConfig& config)
		: m_Config(config)
	{
		TYR_ASSERT(s_Instance == nullptr);
		s_Instance = this;

		m_TransferBuffers.Reserve(config.maxTransferBuffers);
	}

	ResourceManager::~ResourceManager()
	{
		s_Instance = nullptr;
	}

	ResourceManager* ResourceManager::Get()
	{
		TYR_ASSERT(s_Instance != nullptr);
		return s_Instance;
	}
}