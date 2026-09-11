#include "WorldManager.h"
#include "BuildConfig.h"
#include "RendererModule.h"
#include "Rendering/RendererAPI.h"

namespace tyr
{
	WorldManager::WorldManager()
	{
		RendererModule* rendererModule;
		TYR_GET_MODULE(RendererModule, rendererModule);
		m_RendererAPI = rendererModule->GetRendererAPI();
	}

	WorldManager::~WorldManager()
	{
		RemoveWorlds();
	}

	void WorldManager::Update(float deltaTime)
	{
		for (Handle worldHandle : m_Worlds)
		{
			World& world = m_WorldPool[worldHandle];
			world.Update(deltaTime);
		}
	}

	Handle WorldManager::AddWorld(const WorldConfig& params)
	{
		const Handle worldHandle = m_WorldPool.Create();
		World& world = m_WorldPool[worldHandle];
		world.Initialize(params);
		m_Worlds.Add(worldHandle);
		return worldHandle;
	}

	const World& WorldManager::GetWorld(Handle worldHandle) const
	{
		return m_WorldPool[worldHandle];
	}

	World& WorldManager::GetWorld(Handle worldHandle)
	{
		return m_WorldPool[worldHandle];
	}

	void WorldManager::RemoveWorld(Handle worldHandle)
	{
		World& world = m_WorldPool[worldHandle];
		world.Shutdown();
		for (uint i = 0; i < m_Worlds.Size(); ++i)
		{
			if (worldHandle == m_Worlds[i])
			{
				m_Worlds.Erase(i);
				break;
			}
		}
		m_WorldPool.Delete(worldHandle);
	}

	void WorldManager::RemoveWorlds()
	{
		for (Handle worldHandle : m_Worlds)
		{
			World& world = m_WorldPool[worldHandle];
			world.Shutdown();
			m_WorldPool.Delete(worldHandle);
		}
		m_Worlds.Clear();
	}
}