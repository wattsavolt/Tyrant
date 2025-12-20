#include "WorldManager.h"
#include "BuildConfig.h"
#include "RendererModule.h"
#include "Rendering/Renderer.h"

namespace tyr
{
	WorldManager::WorldManager()
	{
		RendererModule* rendererModule;
		TYR_GET_MODULE(RendererModule, rendererModule);
		m_Renderer = rendererModule->GetRenderer();
	}

	WorldManager::~WorldManager()
	{
		RemoveWorlds();
	}

	void WorldManager::Update(float deltaTime)
	{
		RenderFrame& renderFrame = m_Renderer->GetRenderFrame();
		renderFrame.Clear();

		// TODO: Set this if window has resized
		renderFrame.windowResize = false;
		for (World* world : m_Worlds)
		{
			world->Update(deltaTime, renderFrame.sceneFrames[world->GetSceneIndex()]);
		}
	}

	World* WorldManager::AddWorld(const WorldConfig& params)
	{
		uint8 sceneIndex = m_Renderer->AddScene();
		uint index;
		World* world = m_WorldPool.Create(index);
		world->Initialize(params, sceneIndex);
		m_Worlds.Add(world);
		return world;
	}

	void WorldManager::RemoveWorld(World* world)
	{
		world->Shutdown();
		for (uint i = 0; i < m_Worlds.Size(); ++i)
		{
			if (world == m_Worlds[i])
			{
				m_Worlds.Erase(i);
				break;
			}
		}
		m_WorldPool.Delete(world);
	}

	void WorldManager::RemoveWorlds()
	{
		for (World* world : m_Worlds)
		{
			world->Shutdown();
			m_WorldPool.Delete(world);
		}
		m_Worlds.Clear();
	}
}