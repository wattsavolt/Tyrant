#include "WorldManager.h"
#include "BuildConfig.h"
#include "RendererModule.h"
#include "Rendering/Renderer.h"

namespace tyr
{
	WorldManager::WorldManager()
		:  m_RenderFrameIndex(0)
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
		RenderFrame& renderFrame = m_RenderFrames[m_RenderFrameIndex];
		renderFrame.Clear();

		// TODO: Set this if window has resized
		renderFrame.windowResize = false;
		for (World* world : m_Worlds)
		{
			world->Update(deltaTime, renderFrame.sceneFrames[world->GetSceneIndex()]);
		}

		m_Renderer->TryAddFrame(&renderFrame);

		m_RenderFrameIndex = (m_RenderFrameIndex + 1) % RenderFrame::c_MaxRenderFrames;
	}

	World* WorldManager::AddWorld(const WorldParams& params)
	{
		uint16 index;
		World* world = m_WorldPool.Create(index);
		world->Initialize(params);
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